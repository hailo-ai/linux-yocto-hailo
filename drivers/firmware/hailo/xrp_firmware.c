// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * xrp_firmware: firmware manipulation for the XRP
 *
 * Copyright (c) 2015 - 2017 Cadence Design Systems, Inc.
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 */

#include "xrp_firmware.h"

#include "xrp_hw.h"
#include "xrp_kernel_dsp_interface.h"

#include <linux/dma-mapping.h>
#include <linux/elf.h>
#include <linux/firmware.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>

#ifndef OF_BAD_ADDR
#define OF_BAD_ADDR (-1ul)
#endif

static phys_addr_t xrp_translate_to_cpu(struct xvp *xvp, Elf32_Phdr *phdr)
{
#if IS_ENABLED(CONFIG_OF)
    phys_addr_t res;
    __be32 addr = cpu_to_be32((u32)phdr->p_paddr);
    struct device_node *node =
        of_get_next_child(xvp->dev->of_node, NULL);

    if (!node)
        node = xvp->dev->of_node;

    res = of_translate_address(node, &addr);

    if (node != xvp->dev->of_node)
        of_node_put(node);
    return res;
#else
    return phdr->p_paddr;
#endif
}

static int xrp_load_segment_to_sysmem(struct xvp *xvp, Elf32_Phdr *phdr)
{
    phys_addr_t pa = xrp_translate_to_cpu(xvp, phdr);
    struct page *page = pfn_to_page(__phys_to_pfn(pa));
    size_t page_offs = pa & ~PAGE_MASK;
    size_t offs;

    for (offs = 0; offs < phdr->p_memsz; ++page) {
        void *p = kmap(page);
        size_t sz;

        if (!p)
            return -ENOMEM;

        page_offs &= ~PAGE_MASK;
        sz = PAGE_SIZE - page_offs;

        if (offs < phdr->p_filesz) {
            size_t copy_sz = sz;

            if (phdr->p_filesz - offs < copy_sz)
                copy_sz = phdr->p_filesz - offs;

            copy_sz = ALIGN(copy_sz, 4);
            memcpy(p + page_offs,
                   (void *)xvp->firmware->data +
                   phdr->p_offset + offs,
                   copy_sz);
            page_offs += copy_sz;
            offs += copy_sz;
            sz -= copy_sz;
        }

        if (offs < phdr->p_memsz && sz) {
            if (phdr->p_memsz - offs < sz)
                sz = phdr->p_memsz - offs;

            sz = ALIGN(sz, 4);
            memset(p + page_offs, 0, sz);
            page_offs += sz;
            offs += sz;
        }
        kunmap(page);
    }
    dma_sync_single_for_device(xvp->dev, pa, phdr->p_memsz, DMA_TO_DEVICE);
    return 0;
}

static int xrp_load_segment_to_iomem(struct xvp *xvp, Elf32_Phdr *phdr)
{
    phys_addr_t pa = xrp_translate_to_cpu(xvp, phdr);
    void __iomem *p = ioremap(pa, phdr->p_memsz);

    if (!p) {
        dev_err(xvp->dev,
            "couldn't ioremap %pap x 0x%08x\n",
            &pa, (u32)phdr->p_memsz);
        return -EINVAL;
    }
    
    xrp_memcpy_tohw(p, (void *)xvp->firmware->data + phdr->p_offset, phdr->p_filesz);
    xrp_memset_hw(p + phdr->p_filesz, 0, phdr->p_memsz - phdr->p_filesz);

    iounmap(p);
    return 0;
}

static int xrp_load_segment(struct xvp *xvp, Elf32_Phdr *phdr)
{
    phys_addr_t pa;

    pa = xrp_translate_to_cpu(xvp, phdr);
    if (pa == (phys_addr_t)OF_BAD_ADDR) {
        dev_err(xvp->dev,
            "device address 0x%08x could not be mapped to host physical address",
            (u32)phdr->p_paddr);
        return -EINVAL;
    }

    if (!is_valid_fw_addr(xvp, pa)) {
        dev_err(xvp->dev, "invalid fw addr in elf 0x%llx\n", pa);
        return -EINVAL;
    }

    dev_dbg(xvp->dev, "loading segment (device 0x%08x) to physical %pap\n",
        (u32)phdr->p_paddr, &pa);

    if (pfn_valid(__phys_to_pfn(pa)))
        return xrp_load_segment_to_sysmem(xvp, phdr);
    else
        return xrp_load_segment_to_iomem(xvp, phdr);
}

static inline bool xrp_section_bad(struct xvp *xvp, const Elf32_Shdr *shdr)
{
    return shdr->sh_offset > xvp->firmware->size ||
        shdr->sh_size > xvp->firmware->size - shdr->sh_offset;
}

static int xrp_firmware_find_symbol(struct xvp *xvp, const char *name,
    void **paddr, size_t *psize)
{
    const Elf32_Ehdr *ehdr = (Elf32_Ehdr *)xvp->firmware->data;
    const void *shdr_data = xvp->firmware->data + ehdr->e_shoff;
    const Elf32_Shdr *sh_symtab = NULL;
    const Elf32_Shdr *sh_strtab = NULL;
    const void *sym_data;
    const void *str_data;
    const Elf32_Sym *esym;
    void *addr = NULL;
    unsigned i;

    if (ehdr->e_shoff == 0) {
        dev_dbg(xvp->dev, "%s: no section header in the firmware image",
            __func__);
        return -ENOENT;
    }
    if (ehdr->e_shoff > xvp->firmware->size ||
        ehdr->e_shnum * ehdr->e_shentsize > xvp->firmware->size - ehdr->e_shoff) {
        dev_err(xvp->dev, "%s: bad firmware SHDR information",
            __func__);
        return -EINVAL;
    }

    /* find symbols */

    for (i = 0; i < ehdr->e_shnum; ++i) {
        const Elf32_Shdr *shdr = shdr_data + i * ehdr->e_shentsize;

        if (shdr->sh_type == SHT_SYMTAB) {
            sh_symtab = shdr;
            if (sh_symtab->sh_link < ehdr->e_shnum) {
                const Elf32_Shdr *shdr = shdr_data +
                    sh_symtab->sh_link * ehdr->e_shentsize;

                if (shdr->sh_type == SHT_STRTAB)
                    sh_strtab = shdr;
            }
            break;
        }
    }

    if (!sh_symtab || !sh_strtab) {
        dev_dbg(xvp->dev, "%s: no symtab or strtab in the firmware image",
            __func__);
        return -ENOENT;
    }

    if (xrp_section_bad(xvp, sh_symtab)) {
        dev_err(xvp->dev, "%s: bad firmware SYMTAB section information",
            __func__);
        return -EINVAL;
    }

    if (xrp_section_bad(xvp, sh_strtab)) {
        dev_err(xvp->dev, "%s: bad firmware STRTAB section information",
            __func__);
        return -EINVAL;
    }

    /* iterate through all symbols, searching for the name */

    sym_data = xvp->firmware->data + sh_symtab->sh_offset;
    str_data = xvp->firmware->data + sh_strtab->sh_offset;

    for (i = 0; i < sh_symtab->sh_size; i += sh_symtab->sh_entsize) {
        esym = sym_data + i;

        if (!(ELF_ST_TYPE(esym->st_info) == STT_OBJECT &&
              esym->st_name < sh_strtab->sh_size &&
              strncmp(str_data + esym->st_name, name,
                  sh_strtab->sh_size - esym->st_name) == 0))
            continue;

        if (esym->st_shndx > 0 && esym->st_shndx < ehdr->e_shnum) {
            const Elf32_Shdr *shdr = shdr_data +
                esym->st_shndx * ehdr->e_shentsize;
            Elf32_Off in_section_off = esym->st_value - shdr->sh_addr;

            if (xrp_section_bad(xvp, shdr)) {
                dev_err(xvp->dev, "%s: bad firmware section #%d information",
                    __func__, esym->st_shndx);
                return -EINVAL;
            }

            if (esym->st_value < shdr->sh_addr ||
                in_section_off > shdr->sh_size ||
                esym->st_size > shdr->sh_size - in_section_off) {
                dev_err(xvp->dev, "%s: bad symbol information",
                    __func__);
                return -EINVAL;
            }
            addr = (void *)xvp->firmware->data + shdr->sh_offset +
                in_section_off;

            dev_dbg(xvp->dev,
                "%s: found symbol '%s', st_shndx = %d, sh_offset = 0x%08x, "
                "sh_addr = 0x%08x, st_value = 0x%08x, address = %p",
                __func__, (char *)str_data + esym->st_name,
                esym->st_shndx, shdr->sh_offset, shdr->sh_addr,
                esym->st_value, addr);
        } else {
            dev_dbg(xvp->dev, "%s: unsupported section index in found symbol: 0x%x",
                __func__, esym->st_shndx);
            return -EINVAL;
        }
        break;
    }

    if (!addr)
        return -ENOENT;

    *paddr = addr;
    *psize = esym->st_size;

    return 0;
}

static int xrp_firmware_fixup_symbol(struct xvp *xvp, const char *name,
    phys_addr_t v)
{
    void *addr;
    size_t sz;
    int rc;

    rc = xrp_firmware_find_symbol(xvp, name, &addr, &sz);
    if (rc < 0) {
        dev_err(xvp->dev, "%s: symbol \"%s\" is not found",
            __func__, name);
        return rc;
    }

    if (sz != sizeof(u32)) {
        dev_err(xvp->dev, "%s: symbol \"%s\" has wrong size: %zu",
            __func__, name, sz);
        return -EINVAL;
    }

    /* update data associated with symbol */
    dev_dbg(xvp->dev, "%s: modifying '%s': 0x%x -> 0x%llx",
        __func__, name, *(int32_t *)addr, v);
    memcpy(addr, &v, sz);

    return 0;
}

static int xrp_firmware_fixup_symbols(struct xvp *xvp)
{
    int rc;

    rc = xrp_firmware_fixup_symbol(xvp, "xrp_dsp_comm_base", xvp->comm.dsp_paddr);
    if (rc < 0) {
        return rc;
    }

    rc = xrp_firmware_fixup_symbol(
        xvp, "g_cyclic_log__buffer", xvp->cyclic_log.dsp_paddr);
    if (rc < 0) {
        return rc;
    }

    rc = xrp_firmware_fixup_symbol(
        xvp, "g_cyclic_log__buffer_size", xvp->cyclic_log.size);
    if (rc < 0) {
        return rc;
    }

    return rc;
}

static int xrp_prepare_firmware(struct xvp *xvp)
{
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)xvp->firmware->data;

    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
        dev_err(xvp->dev, "bad firmware ELF magic\n");
        return -EINVAL;
    }

    if (ehdr->e_type != ET_EXEC) {
        dev_err(xvp->dev, "bad firmware ELF type\n");
        return -EINVAL;
    }

    if (ehdr->e_machine != EM_XTENSA) {
        dev_err(xvp->dev, "bad firmware ELF machine\n");
        return -EINVAL;
    }

    if (ehdr->e_phoff >= xvp->firmware->size ||
        ehdr->e_phoff +
        ehdr->e_phentsize * ehdr->e_phnum > xvp->firmware->size) {
        dev_err(xvp->dev, "bad firmware ELF PHDR information\n");
        return -EINVAL;
    }
    
    xvp->reset_vector_address = ehdr->e_entry;
    
    return xrp_firmware_fixup_symbols(xvp);
}

int xrp_load_firmware(struct xvp *xvp)
{
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)xvp->firmware->data;
    int i;

    for (i = 0; i < ehdr->e_phnum; ++i) {
        Elf32_Phdr *phdr = (void *)xvp->firmware->data +
            ehdr->e_phoff + i * ehdr->e_phentsize;
        int rc;

        /* Only load non-empty loadable segments, R/W/X */
        if (!(phdr->p_type == PT_LOAD && (phdr->p_flags & (PF_X | PF_R | PF_W))
            && phdr->p_memsz > 0))
            continue;

        if (phdr->p_offset >= xvp->firmware->size ||
            phdr->p_offset + phdr->p_filesz > xvp->firmware->size) {
            dev_err(xvp->dev, "bad firmware ELF program header entry %d\n", i);
            return -EINVAL;
        }

        dev_dbg(xvp->dev, "loading segment %d\n", i);

        rc = xrp_load_segment(xvp, phdr);
        if (rc < 0)
            return rc;
    }
    return 0;
}

int xrp_init_firmware(struct xvp *xvp)
{
    int ret;

    if (!xvp->firmware) {
        // passing NULL buffer will cause a new buffer of the correct size to be allocated
        // this doesn't happen in request_firmware_into_buf() though. see - kernel_read_file()
        ret = request_firmware_into_buf(&xvp->firmware, xvp->firmware_name, xvp->dev, NULL, 0);
        if (ret < 0)
            return ret;

        ret = xrp_prepare_firmware(xvp);
        if (ret < 0)
            goto err;
    }

err:
    if (ret < 0)
        xrp_release_firmware(xvp);
    return ret;
}

void xrp_release_firmware(struct xvp *xvp)
{
    if (xvp->firmware) {
        release_firmware(xvp->firmware);
        xvp->firmware = NULL;
    }
}

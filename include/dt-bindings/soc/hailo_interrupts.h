#ifndef _DT_BINDINGS_HAILO_INTERRUPTS
#define _DT_BINDINGS_HAILO_INTERRUPTS

#if HAILO_SOC_TYPE == HAILO_SOC_HAILO15
#include <dt-bindings/soc/hailo15_interrupts.h>
#elif HAILO_SOC_TYPE == HAILO_SOC_HAILO15L
#include <dt-bindings/soc/hailo15l_interrupts.h>
#elif HAILO_SOC_TYPE == HAILO_SOC_HAILO12L
#include <dt-bindings/soc/hailo12l_interrupts.h>
#else
#error "Unsupported HAILO_SOC_TYPE"
#endif

#endif /* _DT_BINDINGS_HAILO_INTERRUPTS */
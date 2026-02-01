/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2024 Hailo Technologies Ltd. All rights reserved.
 */

#ifndef HAILO15_VIDEO_PATH_H
#define HAILO15_VIDEO_PATH_H

#define HAILO15_CSI_0 0
#define HAILO15_CSI_1 1
#define HAILO15_CSI_MAX 2

#define HAILO15_SENSORS_PER_CSI (4)
/*
 * Video Group Table:
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | ID | Video Group Name               | Device Node  | CSI ID| Path          | Used for  | Pipes & VCs                  |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * |-1  | HAILO15_VID_GRP_INVALID        | -            | -     | -             | -         |                              |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 0  | HAILO15_VID_GRP_SX_CSI0_ISP_MP | /dev/video0  | CSI-0 | ISP Main-path | SDR, HDR  |                              |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 1  | HAILO15_VID_GRP_SX_CSI0_ISP_SP | /dev/video1  | CSI-0 | ISP Self-path | SDR, HDR  |                              |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 2  | HAILO15_VID_GRP_SX_CSI0_P2A    | /dev/video2  | CSI-0 | Pixel2Axi     | SDR       | pipes-vc[{0    }] = All VCs  |
 * |    |                                |              |       |               | HDR       | pipes-vc[{0,1,2}] = {0,1,2}  |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 3  | HAILO15_VID_GRP_SX_CSI1_ISP_MP | /dev/video3  | CSI-1 | ISP Main-path | SDR, HDR  |                              |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 4  | HAILO15_VID_GRP_SX_CSI1_ISP_SP | /dev/video4  | CSI-1 | ISP Self-path | SDR, HDR  |                              |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 5  | HAILO15_VID_GRP_SX_CSI1_P2A    | /dev/video5  | CSI-1 | Pixel2Axi     | SDR       | pipes-vc[{0    }] = All VCs  |
 * |    |                                |              |       |               | HDR       | pipes-vc[{0,1,2}] = {0,1,2}  |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 6  | HAILO15_VID_GRP_MCM_RAW_WR     | /dev/video6  | CSI-0 | MCM raw write | Raw12     | sensor0 MCM raw capture     |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 10 | HAILO15_VID_GRP_MCM_IN         | /dev/video10 | -     | MCM input     |           |                              |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 20 | HAILO15_VID_GRP_S0_CSI0_P2A    | /dev/video20 | CSI-0 | Pixel2Axi     | SDR       | pipe-vc[0]=0                 |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 21 | HAILO15_VID_GRP_S1_CSI0_P2A    | /dev/video21 | CSI-0 | Pixel2Axi     | SDR       | pipe-vc[1]=1                 |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 22 | HAILO15_VID_GRP_S2_CSI0_P2A    | /dev/video22 | CSI-0 | Pixel2Axi     | SDR       | pipe-vc[2]=2                 |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 23 | HAILO15_VID_GRP_S3_CSI0_P2A    | /dev/video23 | CSI-0 | Pixel2Axi     | SDR       | pipe-vc[3]=3                 |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 50 | HAILO15_VID_GRP_S0_CSI1_P2A    | /dev/video50 | CSI-1 | Pixel2Axi     | SDR       | pipe-vc[0]=0                 |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 51 | HAILO15_VID_GRP_S1_CSI1_P2A    | /dev/video51 | CSI-1 | Pixel2Axi     | SDR       | pipe-vc[1]=1                 |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 52 | HAILO15_VID_GRP_S2_CSI1_P2A    | /dev/video52 | CSI-1 | Pixel2Axi     | SDR       | pipe-vc[2]=2                 |
 * +----+--------------------------------+--------------+-------+---------------+-----------+------------------------------+
 * | 53 | HAILO15_VID_GRP_S3_CSI1_P2A    | /dev/video53 | CSI-1 | Pixel2Axi     | SDR       | pipe-vc[3]=3                 |
 * +----+--------------------------------+--------------+--------------+--------+------------------------------------------+
 */

#define HAILO15_VID_GRP_INVALID          (-1)
#define HAILO15_VID_GRP_SX_CSI0_ISP_MP   (0)
#define HAILO15_VID_GRP_SX_CSI0_ISP_SP   (1)
#define HAILO15_VID_GRP_SX_CSI0_P2A      (2)
#define HAILO15_VID_GRP_SX_CSI1_ISP_MP   (3)
#define HAILO15_VID_GRP_SX_CSI1_ISP_SP   (4)
#define HAILO15_VID_GRP_SX_CSI1_P2A      (5)
#define HAILO15_VID_GRP_MCM_RAW_WR       (6)

#define HAILO15_VID_GRP_MCM_IN           (10)

#define HAILO15_VID_GRP_SX_MAX           (HAILO15_VID_GRP_MCM_IN + 1)

#define HAILO15_VID_GRP_P2A(_base, _sensor) (_base*10 + _sensor)
#define HAILO15_VID_GRP_S0_CSI0_P2A HAILO15_VID_GRP_P2A(HAILO15_VID_GRP_SX_CSI0_P2A,0)
#define HAILO15_VID_GRP_S1_CSI0_P2A HAILO15_VID_GRP_P2A(HAILO15_VID_GRP_SX_CSI0_P2A,1)
#define HAILO15_VID_GRP_S2_CSI0_P2A HAILO15_VID_GRP_P2A(HAILO15_VID_GRP_SX_CSI0_P2A,2)
#define HAILO15_VID_GRP_S3_CSI0_P2A HAILO15_VID_GRP_P2A(HAILO15_VID_GRP_SX_CSI0_P2A,3)
#define HAILO15_VID_GRP_S0_CSI1_P2A HAILO15_VID_GRP_P2A(HAILO15_VID_GRP_SX_CSI1_P2A,0)
#define HAILO15_VID_GRP_S1_CSI1_P2A HAILO15_VID_GRP_P2A(HAILO15_VID_GRP_SX_CSI1_P2A,1)
#define HAILO15_VID_GRP_S2_CSI1_P2A HAILO15_VID_GRP_P2A(HAILO15_VID_GRP_SX_CSI1_P2A,2)
#define HAILO15_VID_GRP_S3_CSI1_P2A HAILO15_VID_GRP_P2A(HAILO15_VID_GRP_SX_CSI1_P2A,3)

#define HAILO15_VID_GRP_MAX    (HAILO15_VID_GRP_S3_CSI1_P2A + 1)

#endif /* HAILO15_VIDEO_PATH_H */

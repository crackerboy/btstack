/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */
 
// *****************************************************************************
//
// SBC decoder tests
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "btstack.h"
#include "btstack_cvsd_plc.h"

#include "wav_util.h"

#define BYTES_PER_FRAME 2

static char wav_filename[1000];
static char pklg_filename[1000];

static btstack_cvsd_plc_state_t plc_state;

static void show_usage(void){
    printf("\n\nUsage: ./pklg_cvsd_test input_file\n");
    printf("Example: ./pklg_cvsd_test pklg/test1\n");
}

static ssize_t __read(int fd, void *buf, size_t count){
    ssize_t len, pos = 0;

    while (count > 0) {
        len = read(fd, (int8_t * )buf + pos, count);
        if (len <= 0)
            return pos;

        count -= len;
        pos   += len;
    }
    return pos;
}

int main (int argc, const char * argv[]){
    if (argc < 2){
        show_usage();
        return -1;
    }
    
    int argv_pos = 1;
    const char * filename = argv[argv_pos++];
    
    strcpy(pklg_filename, filename);
    strcat(pklg_filename, ".pklg");

    strcpy(wav_filename, filename);
    strcat(wav_filename, ".wav");


    int fd = open(pklg_filename, O_RDONLY);
    if (fd < 0) {
        printf("Can't open file %s", pklg_filename);
        return -1;
    }
    printf("Open pklg file: %s\n", pklg_filename);
 
    wav_writer_open(wav_filename, 1, 8000);

    btstack_cvsd_plc_init(&plc_state);
    
    int sco_packet_counter = 0;
    while (1){
        int bytes_read;
        // get next packet
        uint8_t header[13];
        bytes_read = __read(fd, header, sizeof(header));
        if (0 >= bytes_read) break;

        uint8_t packet[256];
        uint32_t size = big_endian_read_32(header, 0) - 9;
        bytes_read = __read(fd, packet, size);

        uint8_t type = header[12];
        if (type != 9) continue;
        sco_packet_counter++;

        int16_t audio_frame_out[128];    // 

        if (size > sizeof(audio_frame_out)){
            printf("sco_demo_receive_CVSD: SCO packet larger than local output buffer - dropping data.\n");
            break;
        }

        const int audio_bytes_read = size - 3;
        const int num_samples = audio_bytes_read / BYTES_PER_FRAME;

        // convert into host endian
        int16_t audio_frame_in[128];
        int i;
        for (i=0;i<num_samples;i++){
            audio_frame_in[i] = little_endian_read_16(packet, 3 + i * 2);
        }

        btstack_cvsd_plc_process_data(&plc_state, audio_frame_in, num_samples, audio_frame_out);
        wav_writer_write_int16(num_samples, audio_frame_out);
    }

    wav_writer_close();
    close(fd);
}

#include <fstream>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <fmt/printf.h>

static const char NUT_STRING[] = "nut/multimedia container";

static const uint64_t main_startcode = 0x7A561F5F04ADULL + (((uint64_t)('N'<<8) + 'M')<<48);
static const uint64_t stream_startcode = 0x11405BF2F9DBULL + (((uint64_t)('N'<<8) + 'S')<<48);
static const uint64_t syncpoint_startcode = 0xE4ADEECA4569ULL + (((uint64_t)('N'<<8) + 'K')<<48);
static const uint64_t index_startcode = 0xDD672F23E64EULL + (((uint64_t)('N'<<8) + 'X')<<48);
static const uint64_t info_startcode = 0xAB68B596BA78ULL + (((uint64_t)('N'<<8) + 'I')<<48);

static const uint64_t FLAG_KEY = 1<<0;
static const uint64_t FLAG_CODED_PTS = 1<<3;
static const uint64_t FLAG_STREAM_ID = 1<<4;
static const uint64_t FLAG_SIZE_MSB = 1<<5;
static const uint64_t FLAG_CHECKSUM = 1<<6;
static const uint64_t FLAG_RESERVED = 1<<7;
static const uint64_t FLAG_SM_DATA = 1<<8;
static const uint64_t FLAG_HEADER_IDX = 1<<10;
static const uint64_t FLAG_MATCH_TIME = 1<<11;
static const uint64_t FLAG_CODED = 1<<12;

template <typename T>
static T
readbe(FILE *f)
{
    T val;
    size_t width = sizeof(T);
    for (size_t i = 0; i < width; i++) {
        uint8_t *p = ((uint8_t *) &val) + (width-i-1);
        fread(p, 1, 1, f);
    }
    return val;
}

static uint64_t
readv(FILE *f)
{
    uint64_t val = 0;
    while (1) {
        uint8_t byte;
        fread(&byte, 1, 1, f);
        uint8_t more = (byte & 128) >> 7;
        uint8_t data = byte & 127;
        val = val*128 + data;
        if (!more) return val;
    }
}

int
main(int argc, char *argv[])
{
    fmt::printf("starting simpeg2\n");
    if (argc < 2) {
        fmt::fprintf(stderr, "need input file");
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    char header_buf[sizeof(NUT_STRING)];
    fread(header_buf, 1, sizeof(NUT_STRING), f);
    fmt::printf("header: %s\n", header_buf);
    if (std::string(header_buf) != NUT_STRING) {
        fmt::fprintf(stderr, "not a nut file");
        return 1;
    }

    uint64_t timebasenum, timebasedenom;
    uint64_t flags[256], dataszmul[256], dataszlsb[256];
    // uint64_t headeridx,
    int nframes = 0;

    int ret;
    long nextpos = ftell(f);
    while (1) {
        fseek(f, nextpos, SEEK_SET);

        uint8_t peek;
        ret = fread(&peek, 1, 1, f);
        if (ret < 1) {
            fprintf(stderr, "error reading\n");
            break;
        }
        ungetc(peek, f);

        fmt::printf("peek: %c\n", peek);
        if (peek == 'N') {
            // header
            uint64_t startcode = readbe<uint64_t>(f);
            uint64_t fwdptr = readv(f);
            fmt::printf("fwd: %d\n", fwdptr);
            assert(fwdptr <= 4096);
            nextpos = ftell(f) + fwdptr;

            uint64_t version, streamcnt, maxdist, timebasecnt;
            int i, j;
            uint64_t tmpflag, tmpfields;
            uint64_t tmppts, tmpmul, tmpstream, tmpsz, tmpres, tmpcnt;

            switch (startcode) {
            case main_startcode:
                fmt::printf("main\n");
                version = readv(f);
                streamcnt = readv(f);
                maxdist = readv(f);
                timebasecnt = readv(f);
                assert(streamcnt == 1 && timebasecnt == 1);
                timebasenum = readv(f);
                timebasedenom = readv(f);
                fmt::printf("version: %d streamcnt: %d maxdist: %d timebasecnt: %d num: %d denom: %d\n",
                    version, streamcnt, maxdist, timebasecnt, timebasenum, timebasedenom);
                for (i = 0; i < 256;) {
                    tmpflag = readv(f);
                    tmpfields = readv(f);
                    fmt::printf("flag starting at %d: fields: %d\n", i, tmpfields);
                    if (tmpfields > 0) tmppts = readv(f);
                    if (tmpfields > 1) tmpmul = readv(f);
                    if (tmpfields > 2) tmpstream = readv(f);
                    if (tmpfields > 3) tmpsz = readv(f); else tmpsz = 0;
                    if (tmpfields > 4) tmpres = readv(f); else tmpres = 0;
                    if (tmpfields > 5) tmpcnt = readv(f); else tmpcnt = tmpmul - tmpsz;
                    fmt::printf("  pts: %d mul: %d stream: %d sz: %d res: %d cnt: %d\n",
                        tmppts, tmpmul, tmpstream, tmpsz, tmpres, tmpcnt);
                    for (j = 0; j < tmpcnt && i < 256; j++, i++) {
                        if (i == 'N') {
                            fmt::printf("  invalid\n");
                            j--;
                        } else {
                            flags[i] = tmpflag;
                            dataszmul[i] = tmpmul;
                            dataszlsb[i] = tmpsz + j;
                        }
                    }
                }
                break;
            case stream_startcode: fmt::printf("stream\n"); break;
            case syncpoint_startcode: fmt::printf("syncpoint\n"); break;
            case index_startcode: fmt::printf("index\n"); break;
            case info_startcode: fmt::printf("info\n"); break;
            default: fmt::printf("unknown startcode\n");
            }
            fseek(f, fwdptr, SEEK_CUR);
        } else {
            // frame
            fmt::printf("frame\n");
            uint64_t framecode = readbe<uint8_t>(f);
            fmt::printf("frame code %d\n", framecode);
            uint64_t frameflags = flags[framecode];
            uint64_t dataszmsb = 0;
            if (frameflags & FLAG_KEY) {
                fmt::printf("keyframe!\n");
            }
            if (frameflags & FLAG_CODED) {
                fmt::printf("coded\n");
                assert(0);
            }
            if (frameflags & FLAG_STREAM_ID) {
                fmt::printf("stream id\n");
                assert(0);
            }
            if (frameflags & FLAG_CODED_PTS) {
                fmt::printf("coded pts\n");
                assert(0);
            }
            if (frameflags & FLAG_SIZE_MSB) {
                fmt::printf("size msb\n");
                dataszmsb = readv(f);
                fmt::printf("dataszmsb: %d\n", dataszmsb);
            }
            if (frameflags & FLAG_MATCH_TIME) {
                fmt::printf("match time\n");
                assert(0);
            }
            if (frameflags & FLAG_HEADER_IDX) {
                fmt::printf("header idx\n");
                assert(0);
            }
            if (frameflags & FLAG_RESERVED) {
                fmt::printf("reserved\n");
                assert(0);
            }
            if (frameflags & FLAG_CHECKSUM) {
                fmt::printf("checksum\n");
                assert(0);
            }
            if (frameflags & FLAG_SM_DATA) {
                fmt::printf("sm data\n");
                assert(0);
            }
            size_t datasz = dataszlsb[framecode] + dataszmsb * dataszmul[framecode];
            fmt::printf("datasz: %zu\n", datasz);
            uint8_t *databuf = new uint8_t[datasz];
            fread(databuf, 1, datasz, f);

            for (int offset = 0; offset < datasz-2; offset++) {
                uint32_t mpegpfx = *((uint32_t *) (databuf+offset)) & 0xffffff;
                // assert(mpegpfx == 0x010000);
                if (mpegpfx == 0x010000) {
                    uint8_t mpegstartcode = *(databuf+offset+3);
                    // fmt::printf("mpegstartcode: %02x\n", mpegstartcode);
                    fmt::printf("startcode at %d: %02x\n", offset, mpegstartcode);

                    if (mpegstartcode == 0xb5) {
                        uint8_t mpegext = *(databuf+offset+4) & 0xf;
                        fmt::printf("mpegext: %d\n", mpegext);
                        uint8_t sz_ext0 = *(databuf+offset+4+1);
                        uint8_t sz_ext1 = *(databuf+offset+4+2);
                        fmt::printf("w_ext: %d h_ext: %d\n",
                            (((sz_ext0&1)<<1) + (sz_ext1>>7)),
                            (sz_ext1>>5)&0x3);
                    }

                    if (mpegstartcode == 0xb3) {
                        uint32_t res = *((uint32_t *) (databuf+offset+4)) & 0xffffff;
                        fmt::printf("res: %08x\n", res);
                        // uint32_t w = res & 0x000fff;
                        uint8_t wnibble2 = (res&0x0000f0)>>4;
                        uint8_t wnibble1 = (res&0x00000f);
                        uint8_t wnibble0 = (res&0x00f000)>>12;
                        fmt::printf("wnibble0: %d\n", wnibble0);
                        fmt::printf("wnibble1: %d\n", wnibble1);
                        fmt::printf("wnibble2: %d\n", wnibble2);
                        fmt::printf("w: %d\n",
                            (((int) wnibble2)*256) + (wnibble1<<4) + wnibble0);
                    }

                    if (mpegstartcode == 0x00 && (frameflags&FLAG_KEY)) {
                        fmt::printf("keyframe start\n");
                    }

                    if ((frameflags&FLAG_KEY)
                        && mpegstartcode >= 0x01 && mpegstartcode <= 0xAF) {
                        uint8_t slice_byte = *(databuf+offset+4);
                        uint8_t quantiser_scale_code = slice_byte >> 3;
                        uint8_t intra_slice_flag = slice_byte & (1<<2);
                        fmt::printf("quantscalecode: %d isf: %d\n",
                            quantiser_scale_code, intra_slice_flag);
                    }
                }
            }


            delete[] databuf;
            nextpos = ftell(f);
            nframes++;
        }
    }

    fclose(f);
    fmt::printf("got %d frames\n", nframes);
}

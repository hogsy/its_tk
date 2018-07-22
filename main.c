#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <netinet/in.h>
#include <ctype.h>
#include <stdbool.h>

/* fugly code, fugly code for everyone! */

#define DEFAULT_PATH "./ITSDEMO.EXE"

typedef uint16_t word;
typedef uint32_t dword;

#define ReadUINT8(var) \
            memcpy(&(var), pos, sizeof(uint8_t)); \
            printf(" " # var " %u (%#x) (OFF:%lu) (LEN:%lu)\n", (var), (var), pos - data_block + exe_end, sizeof(uint8_t)); \
            pos += sizeof(uint8_t)
#define ReadUINT16(var) \
            memcpy(&(var), pos, sizeof(uint16_t)); \
            printf(" " # var " %u (%#x) (OFF:%lu) (LEN:%lu)\n", (var), (var), pos - data_block + exe_end, sizeof(uint16_t)); \
            pos += sizeof(uint16_t)
#define ReadINT16(var) \
            memcpy(&(var), pos, sizeof(int16_t)); \
            printf(" " # var " %d (%#x) (OFF:%lu) (LEN:%lu)\n", (var), (var), pos - data_block + exe_end, sizeof(int16_t)); \
            pos += sizeof(int16_t)
#define ReadUINT32(var) \
            memcpy(&(var), pos, sizeof(uint32_t)); \
            printf(" " # var " %u (%#x) (OFF:%lu) (LEN:%lu)\n", (var), (var), pos - data_block + exe_end, sizeof(uint32_t)); \
            pos += sizeof(uint32_t)
#define ReadINT32(var) \
            memcpy(&(var), pos, sizeof(int32_t)); \
            printf(" " # var " %d (%#x) (OFF:%lu) (LEN:%lu)\n", (var), (var), pos - data_block + exe_end, sizeof(int32_t)); \
            pos += sizeof(int32_t)
#define ReadSTRING(var, length) \
            strncpy((var), pos, length); \
            printf(" " # var " %s (OFF:%lu) (LEN:%lu)\n", (var), pos - data_block + exe_end, length); \
            pos += length
#define ReadBLOCK(var, length) \
            memcpy((var), pos, length); \
            printf(" " # var " BLOCK (OFF:%lu) (LEN:%lu)\n", pos - data_block + exe_end, length); \
            pos += length
#define SkipBLOCK(length) \
            printf(" Skipping %u bytes\n", (length)); \
            pos += length

int main() {
    FILE *its_file = fopen(DEFAULT_PATH, "rb");
    if(its_file == NULL) {
        printf("failed to load \"" DEFAULT_PATH "\"!\n");
        return EXIT_FAILURE;
    }

    struct stat buf;
    if(stat(DEFAULT_PATH, &buf) != 0) {
        printf("failed to stat, %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    /* load MZ header
     * http://www.fileformat.info/format/exe/corion-mz.htm */
    struct {
        char id[2];

        word num_bytes;
        word num_pages;
        word num_entries;

        word header_size;

        word min_paragraphs;
        word max_paragraphs;

        word initial_ss;
        word initial_sp;

        word checksum;

        dword entry_point;

        word relocation_offset;
        word overlay_number;
    } mz_header;
    printf("expecting header of %lu bytes\n", sizeof(mz_header));
    if(fread(&mz_header, sizeof(mz_header), 1, its_file) != 1) {
        printf("failed to load header!\n");
        return EXIT_FAILURE;
    }

    if(mz_header.id[0] != 'M' && mz_header.id[1] != 'Z') {
        printf("invalid header identifier!\n");
        return EXIT_FAILURE;
    }

    /* expected to be 175869 for ITSDEMO.EXE */
    int exe_end = mz_header.num_pages * 512 - (512 - mz_header.num_bytes);
    if(fseek(its_file, exe_end, SEEK_SET) != 0) {
        printf("failed to seek to end of executable!\n");
        return EXIT_FAILURE;
    }

    size_t buffer_size = (size_t) buf.st_size - exe_end;
    char *data_block = malloc(buffer_size);
    if(data_block == NULL) {
        printf("failed to malloc %ld bytes!\n", buffer_size);
        return EXIT_FAILURE;
    }

    /* read in the first chunk of data */
    if(fread(data_block, sizeof(char), buffer_size, its_file) != buffer_size) {
        printf("failed to load in additional data - possibly end-of-file?\n");
    }

#define GENERATE_SOURCE_TREE
#if defined(GENERATE_SOURCE_TREE)
    mkdir("./source/", 0700);
    mkdir("./source/sound/", 0700);

    struct {
        char name[64];
        char offset_start[5];
        char offset_end[5];
    } functions[2048];
    unsigned int num_functions = 0;
    memset(functions, 0, sizeof(functions));

    struct {
        char path[256];
        char offset[4];
    } source_tree[58];
    unsigned int cur_source_file = 0;
    memset(source_tree, 0, sizeof(source_tree));

    FILE *f_st = fopen("./source_tree", "w");
#endif

    for(unsigned long i = 0; i < buffer_size; ++i) {
        if(strncmp("VMM swap file version", data_block + i, 21) == 0) {
            printf("found swap file at %ld\n", i + exe_end);
            continue;
        }

        if(strncmp("  Address         Publics by Value", data_block + i, 34) == 0) {
            i += 38; /* skip the heading */
            char *cur = (data_block + i);
            for(;;) {
                if(*cur != ' ' && *cur == '\r') {
                    printf("end! (%d functions)\n", num_functions);
                    break;
                }

                cur += 1;  /* skip ' ' */
                snprintf(functions[num_functions].offset_start, 5, "%s", cur);
                printf("FUNCTION (%s:", functions[num_functions].offset_start);
                cur += 5;
                snprintf(functions[num_functions].offset_end, 5, "%s", cur);
                printf("%s)  ", functions[num_functions].offset_end);
                cur += 11;

                char tbuf[64];
                unsigned int pos = 0;
                memset(tbuf, 0, sizeof(tbuf));
                while (*cur != '\r') {
                    tbuf[pos++] = *cur++;
                }

                snprintf(functions[num_functions].name, sizeof(functions[num_functions].name), "%s", tbuf);
                printf("%s\n", tbuf);

                num_functions++;

                cur += 2;
            }
            continue;
        }

        if(strncmp("Line numbers", data_block + i, 12) == 0) {
            char file_name[24];
            char *cur = (data_block + i);
            while(*cur != '(') {
                ++cur;
            }
            if(*cur == '(') {
                ++cur;
                for(unsigned int j = 0; j < 24; ++j, ++cur) {
                    if(*cur == ')') {
                        file_name[j] = '\0';
                        break;
                    }
                    file_name[j] = *cur;
                }
                fprintf(f_st, "%s\n", file_name);
                printf("%s\n", file_name);

#if defined(GENERATE_SOURCE_TREE)
                char new_name[strlen(file_name) - 4];
                for(unsigned int j = 0; j < sizeof(new_name); ++j) {
                    new_name[j] = (char) tolower(file_name[j]);
                } new_name[sizeof(new_name)] = '\0';
                if(new_name[0] == 'g' && new_name[1] == ':') {
                    for(unsigned int j = 0; j < sizeof(new_name) - 2; ++j) {
                        new_name[j] = new_name[j + 2];
                        if(new_name[j] == '\\') {
                            new_name[j] = '/';
                        }
                    }
                    new_name[sizeof(new_name) - 2] = '\0';
                }
                snprintf(source_tree[cur_source_file].path, sizeof(source_tree[cur_source_file].path), "./source/%s.pas", new_name);
                FILE *f_source = fopen(source_tree[cur_source_file].path, "w");
                /* now write some code... */

                fprintf(f_source, "{*\n    ITS Generated Source Tree\n    Not 100%% accurate but should give a rough layout!\n*}\n\n");

                char *pos = (data_block + i);
                while(*pos != '\r') {
                    *pos++;
                }

                pos += 4; /* \n\r\n */

                char start_offset[5];
                memset(start_offset, 0, sizeof(start_offset));


                char *pos_unit = (data_block + i + 18);
                while(*pos_unit != ')') {
                    *pos_unit++;
                } pos_unit += 10;

                char unit_name[64];
                unsigned int length = 0;
                while(*pos_unit != '\r') {
                    unit_name[length++] = *pos_unit++;
                } unit_name[length] = '\0';

                bool is_program = false;
                if(strncmp(unit_name, "PROGRAM", 7) != 0) {
                    fprintf(f_source, "unit %s;\n", unit_name);
                    fprintf(f_source, "interface\n");
                    fprintf(f_source, "implementation\n");
                } else {
                    fprintf(f_source, "PROGRAM tvr;\n\n");
                    is_program = true;
                }

                while(*pos != 'L') {
                    pos += 7;
                    if(!isascii(*pos)) {
                        break;
                    }

                    for(unsigned int j = 0; j < num_functions; ++j) {
                        if(start_offset[0] == '\0') {
                            strncpy(start_offset, pos, 4);
                        }

                        if(strncmp(pos, functions[j].offset_start, 4) == 0 && strncmp(pos + 5, functions[j].offset_end, 4) == 0) {
                            if(functions[j].name[0] == '@') {
                                //fprintf(f_source, "\n");
                                continue;
                            }

                            fprintf(f_source, "{ %s %s:%s ", functions[j].name, functions[j].offset_start, functions[j].offset_end);
                            if(strncmp(start_offset, functions[j].offset_start, 4) != 0) {
                                fprintf(f_source, "import }\n");
                                continue;
                            }

                            fprintf(f_source, "}\n");
                            fprintf(f_source, "procedure %s();\n", functions[j].name);
                            fprintf(f_source, "begin\n");
                            fprintf(f_source, "{ stub }\n");
                            fprintf(f_source, "end;\n\n");
                            break;
                        }
                    }

                    pos += 9;
                    if(*pos == '\r') {
                        pos += 2;
                    }
                }

                if(is_program) {
                    fprintf(f_source, "\nbegin\n");
                }
                fprintf(f_source, "\nend. { %s }\n\n", unit_name);

                /* and done! */
                fclose(f_source);
#endif
            } else {
                printf("failed to find end of file name!\n");
            }
            continue;
        } else if(strncmp("VR-System", data_block + i, 9) == 0) {
            printf("found VR header at %ld\n\n", i + exe_end);

            /* pointer to current position */
            char *pos = (data_block + i);

            char header[50];
            memcpy(header, pos, sizeof(header));
            if(strncmp(pos, "VR-System construction. By Triton Prod. 1994-95.  ", 50) == 0) {
                printf("Construction\n");
            } else if(strncmp(pos, "VR-System object file. By Triton Prod. 1994-95.   ", 50) == 0) {
                printf("Object File\n");
            } else if(strncmp(pos, "VR-System savegame v1.1. By Triton Prod. 1994-95. ", 50) == 0) {
                printf("Save Game\n");
            } else {
                printf("unhandled case!??\n");
            }
            pos += 50;

            uint8_t version;
            ReadUINT8(version);

            /* now we uh, have to try and load in all the chunks :( */

            unsigned int num_rooms = 0;

            while(1) {
                uint8_t name_length;
                memcpy(&name_length, pos, sizeof(uint8_t));
                pos += sizeof(uint8_t);

                char name[name_length];
                strncpy(name, pos, sizeof(name));
                pos += name_length;

                printf("%s (%u)\n", name, name_length);

                if(strncmp(name, "PROJECT", name_length) == 0) {
                    char project_id[2];
                    strncpy(project_id, pos, sizeof(project_id));
                    printf(" ID : %c%c\n", project_id[0], project_id[1]);
                    pos += sizeof(project_id);
                } else if(strncmp(name, "ROOMBSP", name_length) == 0) {
                    uint16_t unknown;
                    memcpy(&unknown, pos, sizeof(uint16_t));
                    printf(" UNKNOWN : %u\n", unknown);
                    pos += sizeof(uint16_t);
                    num_rooms++;
                } else if(strncmp(name, "N-ROOMS", name_length) == 0) {
                    /* possibly depends on the number of ROOMBSP entries?
                     * so 16 bit integer for each ROOMBSP in N-ROOMS...
                     * will never be able to confirm this :( */
                    pos += 20;
                } else if(strncmp(name, "ROOMLST", name_length) == 0) {
                    pos += 96;
                } else if(strncmp(name, "LEVEL", 5) == 0) {
                    /* literally just the label, nothing else */
                } else if(strncmp(name, "BSPTREE", name_length) == 0) {
                    pos += 4;
                } else if(strncmp(name, "PACKCRD", name_length) == 0) {
                    uint16_t unused;
                    memcpy(&unused, pos, sizeof(uint16_t));
                    printf(" UNUSED : %u\n", unused);
                    pos += sizeof(uint16_t);
                } else if(strncmp(name, "UNPACKC", name_length) == 0) {
                    uint16_t unused;
                    memcpy(&unused, pos, sizeof(uint16_t));
                    printf(" UNUSED : %u\n", unused);
                    pos += sizeof(uint16_t);
                } else if(strncmp(name, "SRTWALL", name_length) == 0) {
                    uint16_t length;
                    memcpy(&length, pos, sizeof(uint16_t));
                    printf(" LENGTH : %u\n", length);
                    pos += sizeof(uint16_t) + length;
                } else if(strncmp(name, "CELLPCK", name_length) == 0) {
                    uint16_t length;
                    memcpy(&length, pos, sizeof(uint16_t));
                    printf(" LENGTH : %u\n", length);
                    pos += sizeof(uint16_t) + length;
                } else if(strncmp(name, "BMPWALL", name_length) == 0) {
                    pos += 2;
                } else if(strncmp(name, "FLRMAPS", name_length) == 0) {
                    uint16_t num_floors;
                    memcpy(&num_floors, pos, sizeof(uint16_t));
                    printf(" FLOORS : %u\n", num_floors);
                    pos += sizeof(uint16_t);

                    /* possibly 14 bytes for each tile... */
                    pos += 14 * num_floors;
                } else if(strncmp(name, "TLTMAPS", name_length) == 0) {
                    uint16_t num_floors;
                    memcpy(&num_floors, pos, sizeof(uint16_t));
                    printf(" FLOORS : %u\n", num_floors);
                    pos += sizeof(uint16_t);

                    pos += 52 * num_floors;
                } else if(strncmp(name, "EVNTWLS", name_length) == 0) {
                    uint16_t length;
                    memcpy(&length, pos, sizeof(uint16_t));
                    printf(" LENGTH : %u\n", length);

                    printf("finished!\n");
                    break;
                }

                else {
                    printf("unhandled chunk at %ld, aborting!\n", pos - data_block + exe_end);
                    break;
                }
            }

            printf("\n");
            continue;
        } else if(strncmp("Triton packed sprite info.file", data_block + i, 30) == 0) {
            printf("found sprite info file at %ld\n", i + exe_end);
            continue;
        } else if(strncmp("TRITON Vec.Obj", data_block + i + 1, 14) == 0) {
            /* + 1 here, because the initial 0F byte is actually part of the header :( */
            unsigned long offset = i + exe_end;
            const char *model_name = NULL;
            if(offset == 1564327) {
                model_name = "NIL.HDV";
            } else if(offset == 1564881) {
                model_name = "HUMSHDW.HDV";
            } else if(offset == 1569491) {
                model_name = "ERIK.HDV";
            } else if(offset == 1584541) {
                model_name = "ZOMBIE.HDV";
            } else if(offset == 1596483) {
                model_name = "SKELETON.HDV";
            } else if(offset == 1604393) {
                model_name = "ORK.HDV";
            } else if(offset == 1615663) {
                model_name = "TUNNA.HDV";
            } else if(offset == 1618281) {
                model_name = "1H-CLUB2.HDV";
            } else if(offset == 1619255) {
                model_name = "1H-SWORD.HDV";
            } else if(offset == 1620409) {
                model_name = "2HS-SPER.HDV";
            } else if(offset == 1621575) {
                model_name = "BS-HUM1.HDV";
            }

            printf("\n%s (%ld)\n", model_name, i + exe_end);

            /* pointer to current position */
            char *pos = (data_block + i) + 32;

            /* always the same */
            uint16_t face_offset;
            ReadUINT32(face_offset);

            uint16_t vertex_offset;
            ReadUINT32(vertex_offset);

            uint16_t file_size0;
            ReadUINT32(file_size0);
            uint16_t file_size1;
            ReadUINT32(file_size1);

            uint16_t unknown4;
            ReadUINT32(unknown4);
            uint16_t unknown5;
            ReadUINT32(unknown5);

            uint16_t num_vertices;
            ReadUINT16(num_vertices);

            /* version is located 26 bytes in */
            uint16_t version;
            ReadUINT16(version);

            /* seems to be faces + 2 ? */
            uint32_t num_faces;
            ReadUINT32(num_faces);
            num_faces -= 2;

            /* always 0 */
            uint32_t unused1;
            ReadUINT32(unused1);

            uint32_t unknown6;
            ReadUINT32(unknown6);

            printf(" Skipping to face data at %lu!\n\n", i + exe_end + face_offset);
            pos = (data_block + i) + face_offset;

            typedef struct TVRFace { /* needs to be 36 bytes */
                char u0[13];
                uint16_t vertex_offsets[4];
                char u1[16];
            } TVRFace;

            for(unsigned int j = 0; j < num_faces; ++j) {
                printf("FACE %u\n", j);
                /* quite possibly something to do with the number of faces?
                 * seems to vary between 4 and 5 though
                 * changing it seems to stop parts of the model rendering */
                uint16_t face_u0;
                ReadUINT16(face_u0);

                /* something to do with... vertex colour... ?
                 * ended up making Erik red in the face when changed to 0,
                 * or maybe he's just getting fed up with me fucking him up
                 *
                 * 00 = RED
                 * 0F = GREY
                 * 1F = WHITE (default?) */
                uint8_t face_u1;
                ReadUINT8(face_u1);

                /* totally unused? */
                SkipBLOCK(8);

                /* not 100% ... */
                uint8_t face_tex_mip;
                ReadUINT8(face_tex_mip);

                /* coord offsets */
                uint16_t offset_a;
                ReadUINT16(offset_a);
                uint16_t offset_b;
                ReadUINT16(offset_b);
                uint16_t offset_c;
                ReadUINT16(offset_c);
                uint16_t offset_d;
                ReadUINT16(offset_d);

                printf("grabbing coord info from vertex data...\n");
                unsigned long old_offset = pos - data_block;
                pos = (data_block + i) + vertex_offset + offset_a;

                int32_t x;
                ReadINT32(x);
                int32_t y;
                ReadINT32(y);
                int32_t z;
                ReadINT32(z);

                /* E4       unsure
                 * FE FF FF F8 EA FF FF 40 FC FF FF */

                pos = data_block + old_offset;

                SkipBLOCK(16);

                /* 04 00 1F                     (changes face behaviour?)
                 * 00 00 00 00 00 FA 57 03      (doesn't appear to do anything)
                 * 00                           (has some impact on textures, appears to be unused)
                 *
                 * F4 05                        (offset a)
                 * 30 06                        (offset b)
                 * 1C 08                        (offset c)
                 * 28 08                        (offset d)
                 *
                 * 4A 83 4A 73 3A 74 3A 82 00 00 00 00 00 00 00 00 */
            }

            printf("done!\n\n");

            /* Erik model is 15050 bytes */
            /* model faces are possibly 36 bytes each? */
            /* 10404 bytes for faces in Erik, possibly... */
            /* 289 faces in Erik */
            /* 387 bytes from the last data to where the triangles begin */
            /* 4116 bytes from faces down to next file */
            /* vertex data is likely 12 bytes each */

            /* ZOMBIE
             *  7668 bytes for faces (?)
             *  213 faces */

            mkdir("./CS_VECTR/", 0700);

            char out_path[512];
            snprintf(out_path, sizeof(out_path), "./CS_VECTR/%s", model_name);
            printf("writing %s\n", out_path);
            uint8_t *data = malloc(file_size0);
            memcpy(data, data_block + i, file_size0);
            FILE *f_model = fopen(out_path, "wb");
            fwrite(data, sizeof(uint8_t), file_size0, f_model);
            fclose(f_model);
            continue;
        } else if(strncmp("FORM", data_block + i, 4) == 0) { /* ILBM */
            printf("found ILBM file at %ld, proceeding to export!\n", i);
            /* fetch the size of the ILBM */
            uint32_t length;
            memcpy(&length, data_block + i + 4, sizeof(uint32_t));
            length = ntohl(length);

            /* copy the file into a buffer */
            char out[length + 8];
            memcpy(out, data_block + i, sizeof(out));

            /* now write it to our destination */
            char out_path[512];
            snprintf(out_path, sizeof(out_path), "./%ld.lbm", i + exe_end);
            FILE *out_file = fopen(out_path, "wb");
            if(out_file == NULL) {
                printf("failed to open %s for writing!\n", out_path);
                continue;
            }
            if(fwrite(out, sizeof(out), 1, out_file) != 1) {
                printf("failed to write entire file to %s!\n", out_path);
            }
            fclose(out_file);
            printf("extracted %s!\n", out_path);
            continue;
        } else {
            /* W / H check (checking for image data) */
#if 0
            uint16_t w;
            memcpy(&w, pos, sizeof(uint16_t));
            pos += sizeof(uint16_t);

            uint16_t h;
            memcpy(&h, pos, sizeof(uint16_t));
            pos += sizeof(uint16_t);

            if(w == 32 && h == 32) {
                printf("possible image data at %ld\n", pos - data_block + exe_end);
            }
#endif

#if 0
            if(*(data_block + i) == ';' &&
                    (*(data_block + i + 1) == ' ' || isalnum(*(data_block + i + 1)))) {
                if(i + 128 < buffer_size) {
                    unsigned int s_length = 0;
                    for(unsigned int j = 0; j < 128; ++j, ++s_length) {
                        char *cur = (data_block + i + j);
                        if(strncmp("Extended Module:", cur, 16) == 0) {
                            //printf("hit tracker header while searching for script at %ld\n", i + exe_end);
                            break;
                        } else if(!isprint(*cur) && *cur != '\r' && *cur != '\n') {
                            //printf("hit non alpha-numeric character for script at %ld\n", i + exe_end);
                            break;
                        }

                        if(*cur == '\r' && *(cur + 1) == '\n') {
                            printf("possibly a script line at %ld?\n", i + exe_end);
                            break;
                        }
                    }
                }
            }
#endif
        }
    }

    free(data_block);

    fclose(f_st);
    fclose(its_file);

    return 0;
}
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#define fseek _fseeki64
#define ftell _ftelli64
#endif

const int num_heads        = 16;
const int num_sectors      = 63;
const int sector_size      = 512;

void usage(const char* program_name)
{
    printf("Usage: %s raw-name\n", program_name);
    exit(1);
}

long long file_size(const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error opening '%s'\n", filename);
        exit(2);
    }
    fseek(fp, 0, SEEK_END);
    long long size = ftell(fp);
    fclose(fp);
    return size;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        usage(argv[0]);
    }
    const char* raw_filename = argv[1];

    long long total_size = file_size(raw_filename);
    if (total_size % sector_size) {
        fprintf(stderr, "Error: size is not a multiple of sector size (%d)\n", sector_size);
        exit(3);
    }

    fprintf(stdout, "version=1\n");
    fprintf(stdout, "encoding=\"windows-1252\"\n");
    fprintf(stdout, "CID=fffffffe\n");
    fprintf(stdout, "parentCID=ffffffff\n");
    fprintf(stdout, "isNativeSnapshot=\"no\"\n");
    fprintf(stdout, "createType=\"monolithicFlat\"\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "# Extent description\n");
    fprintf(stdout, "RW %lld FLAT \"%s\" 0\n", total_size / sector_size, raw_filename);
    fprintf(stdout, "\n");
    fprintf(stdout, "# The Disk Data Base \n");
    fprintf(stdout, "#DDB\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "ddb.adapterType = \"ide\"\n");
    fprintf(stdout, "ddb.geometry.cylinders = \"%lld\"\n", total_size / sector_size / num_heads / num_sectors);
    fprintf(stdout, "ddb.geometry.heads = \"%d\"\n", num_heads);
    fprintf(stdout, "ddb.geometry.sectors = \"%d\"\n", num_sectors);
    //fprintf(stdout, "ddb.longContentID = \"196de4c4cf1a20a8d755cd5dfffffffe\"\n");
    //fprintf(stdout, "ddb.uuid = \"60 00 C2 99 fa 16 51 59-f4 7a f3 fa aa e1 e8 94\"\n");
    //fprintf(stdout, "ddb.virtualHWVersion = \"12\"\n");
    return 0;
}

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>

int num_heads     = 16;
int num_sectors   = 63;
int num_cylinders = 104;

int main()
{
    int total_sectors = num_cylinders * num_heads * num_sectors;
    const char* const base_name = "test-vm";
    char desc_filename[256], raw_filename[256];
    snprintf(desc_filename, sizeof(desc_filename), "%s.vmdk", base_name);
    snprintf(raw_filename, sizeof(raw_filename), "%s.raw", base_name);

    FILE* fp = fopen(desc_filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error opening %s\n", desc_filename);
        return 1;
    }
    fprintf(fp, "version=1\n");
    fprintf(fp, "encoding=\"windows-1252\"\n");
    fprintf(fp, "CID=fffffffe\n");
    fprintf(fp, "parentCID=ffffffff\n");
    fprintf(fp, "isNativeSnapshot=\"no\"\n");
    fprintf(fp, "createType=\"monolithicFlat\"\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Extent description\n");
    fprintf(fp, "RW %d FLAT \"%s\" 0\n", total_sectors, raw_filename);
    fprintf(fp, "\n");
    fprintf(fp, "# The Disk Data Base \n");
    fprintf(fp, "#DDB\n");
    fprintf(fp, "\n");
    fprintf(fp, "ddb.adapterType = \"ide\"\n");
    fprintf(fp, "ddb.geometry.cylinders = \"%d\"\n", num_cylinders - 1); // XXX: This is an awful hack... what am I missing?
    fprintf(fp, "ddb.geometry.heads = \"%d\"\n", num_heads);
    fprintf(fp, "ddb.geometry.sectors = \"%d\"\n", num_sectors);
    fprintf(fp, "ddb.longContentID = \"196de4c4cf1a20a8d755cd5dfffffffe\"\n");
    fprintf(fp, "ddb.uuid = \"60 00 C2 99 fa 16 51 59-f4 7a f3 fa aa e1 e8 94\"\n"); // XXX: Don't have a hardcoded UUID
    fprintf(fp, "ddb.virtualHWVersion = \"12\"\n");
    fclose(fp);

    fp = fopen(raw_filename, "r+b");
    if (!fp) {
        fprintf(stderr, "Error opening %s\n", raw_filename);
        return 1;
    }
    fseek(fp, total_sectors*512-1, SEEK_SET);
    fputc(0, fp);
    fclose(fp);
    return 0;
}

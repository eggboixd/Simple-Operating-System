#include "kernel.h"
#include "std_lib.h"
#include "filesystem.h"

void fsInit() {
    struct map_fs map_fs_buf;
    int i = 0;

    readSector((byte*)&map_fs_buf, FS_MAP_SECTOR_NUMBER);
    for (i = 0; i < 16; i++) map_fs_buf.is_used[i] = true;
    for (i = 256; i < 512; i++) map_fs_buf.is_used[i] = true;
    writeSector((byte*)&map_fs_buf, FS_MAP_SECTOR_NUMBER);
}

void fsRead(struct file_metadata* metadata, enum fs_return* status) {
    struct node_fs node_fs_buf;
    struct data_fs data_fs_buf;
    bool found = false;
    int i, j;
    int dataIndex;
    int idx = -1;

    //read filesystem structures 
    readSector(&data_fs_buf, FS_DATA_SECTOR_NUMBER);
    readSector(&(node_fs_buf.nodes[0]), FS_NODE_SECTOR_NUMBER);
    readSector(&(node_fs_buf.nodes[32]), FS_NODE_SECTOR_NUMBER);
    //search for the node 
    for (i = 0; i < FS_MAX_NODE; i++) {
        // Check if node name and parent index match
        if (node_fs_buf.nodes[i].parent_index == metadata->parent_index){
            if(strcmp(node_fs_buf.nodes[i].node_name, metadata->node_name)){
                found = true;
                idx = i;
                break;
            }
        }
    }

    if (!found) {
        //node not found
        *status = FS_R_NODE_NOT_FOUND;
        return;
    }

    //check if the found node is a directory
    if (node_fs_buf.nodes[idx].data_index == FS_NODE_D_DIR) {
        *status = FS_R_TYPE_IS_DIRECTORY;
        return;
    }

    //tnitialize filesize 0
    metadata->filesize = 0;
    dataIndex = node_fs_buf.nodes[idx].data_index;

    //read data sectors
    for (j = 0; j < FS_MAX_SECTOR; j++) {
        //check for end of sectors marker
        if (data_fs_buf.datas[dataIndex].sectors[j] == 0x00)
            break;

        //read sector into metadata buffer
        readSector(metadata->buffer + j * SECTOR_SIZE, data_fs_buf.datas[dataIndex].sectors[j]);

        //update filesize
        metadata->filesize += SECTOR_SIZE;
    }

    //set status to success
    *status = FS_SUCCESS;
}


void fsWrite(struct file_metadata* metadata, enum fs_return* status) {
    struct map_fs map_fs_buf;
    struct node_fs node_fs_buf;
    struct data_fs data_fs_buf;
    int counter = 0;
    int i, j;
    bool found = false;
    int freeNodeIndex = -1;
    int freeDataIndex = -1;
    int sectorsNeeded = metadata->filesize / SECTOR_SIZE + 1;
    int freeBlocks = 0;

    printString("Starting fsWrite\n");

    //read filesystem structures into memory
    readSector((byte*)&map_fs_buf, FS_MAP_SECTOR_NUMBER);
    readSector((byte*)&node_fs_buf, FS_NODE_SECTOR_NUMBER);
    readSector((byte*)&data_fs_buf, FS_DATA_SECTOR_NUMBER);

    //search for existing node
    for (i = 0; i < FS_MAX_NODE * 8; i++) {
        if(strcmp(node_fs_buf.nodes[i].node_name, metadata->node_name)){
            if(node_fs_buf.nodes[i].parent_index == metadata->parent_index){
                printString("Node already exists\n");
                *status = FS_W_NODE_ALREADY_EXISTS;
                return;
            }
        }
    }

    for(i = 0; i < FS_MAX_NODE; i++) {
        if (node_fs_buf.nodes[i].node_name[0] == '\0'){
            if(freeNodeIndex == -1){
                freeNodeIndex = i;
            }
        }
    }

    if (freeNodeIndex == -1) {
        printString("No free node\n");
        *status = FS_W_NO_FREE_NODE;
        return;
    }

    //find a free data 
    for (i = 0; i < FS_MAX_DATA; i++) {
        if (data_fs_buf.datas[i].sectors[0] == 0x00) {
            freeDataIndex = i;
            break;
        }
    }

    //if no free data slot found
    if (freeDataIndex == -1) {
//        printString("No free data slot\n");
        *status = FS_W_NO_FREE_DATA;
        return;
    }

    //count required sectors check for free space 
    for (i = 0; i < SECTOR_SIZE; i++) {
        if (!map_fs_buf.is_used[i]) {
            freeBlocks++;
        }
    }

    if (freeBlocks < sectorsNeeded) {
//        printString("Not enough space\n");
        *status = FS_W_NOT_ENOUGH_SPACE;
        return;
    }

    //update node_fs
    strcpy(node_fs_buf.nodes[freeNodeIndex].node_name, metadata->node_name);
    node_fs_buf.nodes[freeNodeIndex].parent_index = metadata->parent_index;
    node_fs_buf.nodes[freeNodeIndex].data_index = freeDataIndex;
    //allocate sectors in map_fs
    for (i = 0; i < SECTOR_SIZE && counter < sectorsNeeded; i++) {
        if (!map_fs_buf.is_used[i]) {
            data_fs_buf.datas[freeDataIndex].sectors[counter] = i;
            map_fs_buf.is_used[i] = true;
            writeSector(metadata->buffer + counter * SECTOR_SIZE, i);
            counter++;
        }
    }

    //write updated filesystem 
    writeSector((byte*)&map_fs_buf, FS_MAP_SECTOR_NUMBER);
    writeSector((byte*)&node_fs_buf, FS_NODE_SECTOR_NUMBER);
    writeSector((byte*)&data_fs_buf, FS_DATA_SECTOR_NUMBER);

//printString("Write successful\n");
    *status = FS_SUCCESS;
}

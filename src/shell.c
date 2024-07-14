#include "shell.h"
#include "kernel.h"
#include "std_lib.h"
#include "filesystem.h"

void shell() {
    char buf[64];
    char cmd[64];
    char arg[2][64];
    byte cwd = FS_NODE_P_ROOT;
    
    while (true) {
        printString("MengOS:");
        printCWD(cwd);
        printString("$ ");
        
        readString(buf);
        parseCommand(buf, cmd, arg);
        
        if (strcmp(cmd, "cd")) cd(&cwd, arg[0]);
        else if (strcmp(cmd, "ls")) ls(cwd, arg[0]);
        else if (strcmp(cmd, "mv")) mv(cwd, arg[0], arg[1]);
        else if (strcmp(cmd, "cp")) cp(cwd, arg[0], arg[1]);
        else if (strcmp(cmd, "cat")) cat(cwd, arg[0]);
        else if (strcmp(cmd, "mkdir")) mkdir(cwd, arg[0]);
        else if (strcmp(cmd, "clear")) clearScreen();
        else printString("Invalid command\n");
    }
}


void printCWD(byte cwd) {
  struct node_fs node_fs_buf;
  char path[256];  
  char temp[64];  
  int temp_len;
  int path_len = 0;
  int i;
  int stack[256], top = -1;

  readSector(&(node_fs_buf.nodes[0]), FS_NODE_SECTOR_NUMBER);
  readSector(&(node_fs_buf.nodes[32]), FS_NODE_SECTOR_NUMBER + 1);

  if(cwd == FS_NODE_P_ROOT){
    printString("/");
    return;
  }

  while (cwd != FS_NODE_P_ROOT) {
    stack[++top] = cwd;
    cwd = node_fs_buf.nodes[cwd].parent_index;
  }

  while (top != -1){
    cwd = stack[top--];
    strcpy(temp, node_fs_buf.nodes[cwd].node_name);
    temp_len = strlen(temp);

    if(path_len > 0){
      path[path_len++] = '/';
    }

    memcpy((byte*) &path[path_len], (byte*) temp, temp_len);
    path_len += temp_len;
  }

  path[path_len] = '\0';
  printString("/");
  printString(path);
}
// TODO: 5. Implement parseCommand function
void parseCommand(char* buf, char* cmd, char arg[2][64]) {
    int i = 0, j = 0, arg_count = 0;
    while (buf[i] == ' ') i++;

    while(buf[i] != ' ' && buf[i] != '\0'){
        cmd[j++] = buf[i++];
    }
    cmd[j] = '\0';
    for(arg_count = 0; arg_count < 2; arg_count++){
        while (buf[i] == ' ') i++;
        if (buf[i] == '\0') break;

        j = 0;
        while(buf[i] != ' ' && buf[i] != '\0'){
            arg[arg_count][j++] = buf[i++];
        }
        arg[arg_count][j] = '\0';
    }

    for(; arg_count < 2; arg_count++) {
        arg[arg_count][0] = '\0';
    }
}

void cd(byte* cwd, char* dirname) {
    struct node_fs node_fs_buf;
    int i;
    readSector(&(node_fs_buf.nodes[0]), FS_NODE_SECTOR_NUMBER);
    readSector(&(node_fs_buf.nodes[32]), FS_NODE_SECTOR_NUMBER + 1);
    //root directory
    if(strcmp(dirname, "/")) {
        *cwd = FS_NODE_P_ROOT;
        return;
    }
    //parent directory
    if(strcmp(dirname, "..")){
        if (*cwd != FS_NODE_P_ROOT) {
            *cwd = node_fs_buf.nodes[*cwd].parent_index;
        }
        return;
    }
    //check if the dirname exists
    for(i = 0; i < FS_MAX_NODE; i++){
        if(node_fs_buf.nodes[i].parent_index == *cwd && strcmp(node_fs_buf.nodes[i].node_name, dirname)){
            if(node_fs_buf.nodes[i].data_index == FS_NODE_D_DIR){
                *cwd = i;
                return;
            } 
            else{
                printString("not a directory\n");
                return;
            }
        }
    }
    //if no directory is found
    printString("no such file or directory\n");
}

// TODO: 7. Implement ls function
void ls(byte cwd, char* dirname){
    struct node_fs node_fs_buf;
    int i, j;
    bool traverseFile = false;
    readSector(&(node_fs_buf.nodes[0]), FS_NODE_SECTOR_NUMBER);
    readSector(&(node_fs_buf.nodes[32]), FS_NODE_SECTOR_NUMBER + 1);
    for(i = 0; i < FS_MAX_NODE; ++i){
        if(node_fs_buf.nodes[i].parent_index == cwd){
            char* node_name = node_fs_buf.nodes[i].node_name;
            //check node name is not empty
            if (node_name[0] != '\0') {
                bool printable = true;
                //check unprintable Ascii
                for(j = 0; node_name[j] != '\0'; j++){
                    if (node_name[j] < 0x20 || node_name[j] > 0x7E) {
                        printable = false;
                        break;
                    }
                }
                if(printable){
                    traverseFile = true;
                    printString(node_name);
                    printString(" ");
                }
            }
        }
    }
    if(traverseFile) printString("\n");
}

// TODO: 8. Implement mv function
void mv(byte cwd, char* src, char* dst) {
    struct node_fs node_fs_buf;
    int src_node_index = -1, dst_node_index = -1, i;
    bool src_found = false, dst_dir_found = false;
    byte dst_parent_index;
    char dst_filename[64];
    
    //read filesystem node
    readSector(&(node_fs_buf.nodes[0]), FS_NODE_SECTOR_NUMBER);
    readSector(&(node_fs_buf.nodes[32]), FS_NODE_SECTOR_NUMBER + 1);
    
    //find source file 
    for (i = 0; i < FS_MAX_NODE; i++) {
        if (node_fs_buf.nodes[i].parent_index == cwd){
            if(strcmp(node_fs_buf.nodes[i].node_name, src)){
                src_node_index = i;
                src_found = true;
                break;
            }
        }
    }
    
    if (!src_found) {
        printString("Source file not found\n");
        return;
    }
    
    if(node_fs_buf.nodes[src_node_index].data_index == FS_NODE_D_DIR){
        printString("Source is a directory\n");
        return;
    }
    
    //determine parent index and filename
    if (dst[0] == '/') {
        dst_parent_index = FS_NODE_P_ROOT;
        strcpy(dst_filename, dst + 1);
    } 
    else if(dst[0] == '.' && dst[1] == '.' && dst[2] == '/') {
        dst_parent_index = node_fs_buf.nodes[cwd].parent_index;
        strcpy(dst_filename, dst + 3);
    } 
    else{
        for(i = strlen(dst) - 1; i >= 0; i--){
            if (dst[i] == '/') {
                dst[i] = '\0';
                break;
            }
        }
        
        if (i >= 0) {
            dst_parent_index = cwd;
            for (i = 0; i < FS_MAX_NODE; i++) {
                if (node_fs_buf.nodes[i].parent_index == cwd){
                    if(strcmp(node_fs_buf.nodes[i].node_name, dst)){
                        if (node_fs_buf.nodes[i].data_index == FS_NODE_D_DIR){
                            dst_parent_index = i;
                            dst_dir_found = true;
                            break;
                        } 
                        else{
                            printString("Destination is not a directory\n");
                            return;
                        }
                    }
                }
            }
            strcpy(dst_filename, dst + strlen(dst) + 1);
        } 
        else{
            dst_parent_index = cwd;
            strcpy(dst_filename, dst);
        }
    }
    
    if(dst_parent_index == cwd){
        for(i = 0; i < FS_MAX_NODE; i++){
            if(node_fs_buf.nodes[i].parent_index == dst_parent_index){
                if(strcmp(node_fs_buf.nodes[i].node_name, dst_filename)){
                    printString("Destination file already exists\n");
                    return;
                }
            
            }
        }
    }
    
    //mv file
    strcpy(node_fs_buf.nodes[src_node_index].node_name, dst_filename);
    node_fs_buf.nodes[src_node_index].parent_index = dst_parent_index;
    
    //write updated nodesto filesystem
    writeSector(&(node_fs_buf.nodes[0]), FS_NODE_SECTOR_NUMBER);
    writeSector(&(node_fs_buf.nodes[32]), FS_NODE_SECTOR_NUMBER + 1);
    
    printString("File moved successfully\n");
}

// TODO: 9. Implement cp function
void cp(byte cwd, char* src, char* dst) {
    struct file_metadata file_metadata_buf;
    struct node_fs node_fs_buf;
    int src_index = -1, dst_parent_index = -1, i;
    char dst_name[64];
    enum fs_return fs_return_huh;

    // read nodes
    readSector(&(node_fs_buf.nodes[0]), FS_NODE_SECTOR_NUMBER);
    readSector(&(node_fs_buf.nodes[32]), FS_NODE_SECTOR_NUMBER + 1);

    // find source file in cwd
    for (i = 0; i < FS_MAX_NODE; i++) {
        if (node_fs_buf.nodes[i].parent_index == cwd && strcmp(node_fs_buf.nodes[i].node_name, src)) {
            if (node_fs_buf.nodes[i].data_index != FS_NODE_D_DIR) {
                src_index = i;
                break;
            } else {
                printString("cp: source is a directory, not copying\n");
                return;
            }
        }
    }

    if (src_index == -1) {
        printString("cp: source file not found\n");
        return;
    }
    
    // read file system nodes from disk to metadata struct
    file_metadata_buf.parent_index = cwd;
    strcpy(file_metadata_buf.node_name, src);
    fsRead(&file_metadata_buf, &fs_return_huh);
    if (fs_return_huh != FS_SUCCESS) {
        printString("fsRead failed.\n");
        return;
    }

    // find out parent index and filename of the destination
    if (dst[0] == '/') {
        dst_parent_index = FS_NODE_P_ROOT;
        strcpy(dst_name, dst + 1); // Skip the first '/'
    } else if (dst[0] == '.' && dst[1] == '.' && dst[2] == '/') {
        // parent directory
        dst_parent_index = node_fs_buf.nodes[cwd].parent_index;
        strcpy(dst_name, dst + 3); // Skip the "../"
    } else {
        // relative path/subdirectory
        dst_parent_index = cwd;
        for (i = strlen(dst) - 1; i >= 0; i--) {
            if (dst[i] == '/') {
                dst[i] = '\0'; // split to directory and filename
                strcpy(dst_name, dst + i + 1); // filename
                break;
            }
        }
        if (i >= 0) {
            // if directory part exists, find the directory node
            for (i = 0; i < FS_MAX_NODE; i++) {
                if (node_fs_buf.nodes[i].parent_index == cwd && strcmp(node_fs_buf.nodes[i].node_name, dst) && node_fs_buf.nodes[i].data_index == FS_NODE_D_DIR) {
                    dst_parent_index = i;
                    break;
                }
            }
            if (i == FS_MAX_NODE) {
                printString("cp: destination directory not found\n");
                return;
            }
        } else {
            // no directory parth then use the whole dst as filename
            strcpy(dst_name, dst);
        }
    }

    // make sure destination directory is valid
    for (i = 0; i < FS_MAX_NODE; i++) {
        if (node_fs_buf.nodes[i].parent_index == dst_parent_index && strcmp(node_fs_buf.nodes[i].node_name, dst_name)) {
            printString("cp: destination file already exists\n");
            return;
        }
    }

    // copy file data to the new node
    file_metadata_buf.parent_index = dst_parent_index;
    strcpy(file_metadata_buf.node_name, dst_name);

    fsWrite(&file_metadata_buf, &fs_return_huh);

    if (fs_return_huh == FS_SUCCESS) {
        printString("File copied successfully\n");
    } else {
        printString("File copy failed\n");
    }
}

// TODO: 10. Implement cat function
void cat(byte cwd, char* filename){
    // Implementation pending
    struct file_metadata read;
    enum fs_return status;
    read.parent_index = cwd;
    read.filesize = 0;
    strcpy(read.node_name, filename);
    fsRead(&read, &status);

    if(status == FS_R_NODE_NOT_FOUND){
        printString("no such file or directory\n");
        return;
    }
    

    if(status == FS_R_TYPE_IS_DIRECTORY){
        printString("not a file\n");
        return;
    }
    printString("\n");
    printString(read.buffer);
    printString("\n");
}
// TODO: 11. Implement mkdir function
void mkdir(byte cwd, char* dirname) {
    struct node_fs node_fs_buf;
    int free_node_index = -1;
    int i;

    //read node sectors into memory
    readSector(&(node_fs_buf.nodes[0]), FS_NODE_SECTOR_NUMBER);
    readSector(&(node_fs_buf.nodes[32]), FS_NODE_SECTOR_NUMBER + 1);

    //check if a directory with the same name already exists 
    for(i = 0; i < FS_MAX_NODE; i++){
        if(node_fs_buf.nodes[i].parent_index == cwd){
            if(strcmp(node_fs_buf.nodes[i].node_name, dirname)){
                printString("directory already exists\n");
                return;
            }
        }
    }

    //find free node in  filesystem
    for(i = 0; i < FS_MAX_NODE; i++){
        if(node_fs_buf.nodes[i].node_name[0] == '\0'){ 
            free_node_index = i;
            break;
        }
    }

    //print error
    if(free_node_index == -1){
        printString("no free node available\n");
        return;
    }

    //initialize new directory 
    node_fs_buf.nodes[free_node_index].parent_index = cwd;
    node_fs_buf.nodes[free_node_index].data_index = FS_NODE_D_DIR;
    strcpy(node_fs_buf.nodes[free_node_index].node_name, dirname);

    //update node sectors 
    writeSector(&(node_fs_buf.nodes[0]), FS_NODE_SECTOR_NUMBER);
    writeSector(&(node_fs_buf.nodes[32]), FS_NODE_SECTOR_NUMBER + 1);
}


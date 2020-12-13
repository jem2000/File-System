
int main() {
    char * disk_name;
    disk_name = (char *) calloc(16, sizeof(char));
    disk_name[0] = 'a';
    disk_name[1] = 'b';

    char * file_name, * file_name2;
    file_name = (char *) calloc(16, sizeof(char));
    file_name2 = (char *) calloc(16, sizeof(char));
    file_name[0] = 'f';
    file_name[1] = 'n';
    file_name2[0] = 'f';
    file_name2[1] = 'n';
    file_name2[2] = '2';

    if (make_fs(disk_name) == -1)
        return -1;
    if (mount_fs("ab") == -1)
        return 0;

    if (fs_create(file_name) == -1)
        return -3;
    if (fs_create(file_name2) == -1)
        return -3;
    int a = fs_open(file_name);
    printf("a = %d\n", a);
    a = fs_open(file_name2);
    printf("a is now %d\n", a);

    char * buf = (void *) malloc(MAX_FILE_SIZE);
    for (int i = 0; i < MAX_FILE_SIZE - 20; i++) {
        if (i % 5 == 0)
            buf[i] = 'B';
        else if (i % 5 == 1)
            buf[i] = 'o';
        else if (i % 5 == 2)
            buf[i] = 'o';
        else if (i % 5 == 3)
            buf[i] = 'p';
        else if (i % 5 == 4)
            buf[i] = ' ';
    }
    for (int i = MAX_FILE_SIZE - 20; i < MAX_FILE_SIZE; i++) {
        if (i % 5 == 0)
            buf[i] = 'M';
        else if (i % 5 == 1)
            buf[i] = 'e';
        else if (i % 5 == 2)
            buf[i] = 'e';
        else if (i % 5 == 3)
            buf[i] = 'p';
        else if (i % 5 == 4)
            buf[i] = ' ';
    }
    char * buf2 = (char *) malloc(MAX_FILE_SIZE);
    //char buf2[5];
    //buf2[0] = 'a'; buf2[1] = 'b';buf2[2] = 'c';buf2[3] = 'd';buf2[4] = 'e';

    int b = fs_write(1, buf, 17000000);
    printf("write 1 = %d\n", b);
    int size = fs_get_filesize(1);
    printf("size of fildes 1 is %d\n", size);

    fs_lseek(1, 20);

    b = fs_write(0, buf, 17000000);
    printf("write 2 = %d\n", b);
    size = fs_get_filesize(0);
    printf("size of fildes 0 is %d\n", size);

    fs_lseek(1, 0);

    b = fs_read(1, buf2, 17000000);
    printf("read = %d\n", b);

    fs_lseek(0, 0);

    b = fs_read(0, buf, 16000000);
    printf("read 2 = %d\n", b);


    fs_truncate(1, 4090);
    size = fs_get_filesize(1);
    printf("size of fildes 1 is %d\n", size);


    if (umount_fs(disk_name) == 0)
        printf("Unmount successful\n");

    printf("end");
    return 0;
}





int main() {
    int pid = fork();
    if (pid == 0) {
        char *disk_name;
        disk_name = (char *) calloc(16, sizeof(char));
        disk_name[0] = 'a';
        disk_name[1] = 'b';

        char *file_name, *file_name2;
        file_name = (char *) calloc(16, sizeof(char));
        file_name[0] = 'f';
        file_name[1] = 'n';


        if (make_fs(disk_name) == -1)
            return -1;
        if (mount_fs(disk_name) == -1)
            return 0;
        if (fs_create(file_name) == -1)
            return -3;

        int a = fs_open(file_name);
        printf("open in child = %d\n", a);

        char * buf = (char *) malloc(16000000);
        memset(buf, 'q', 16000000);
        int write_check = fs_write(0, buf, 16000000);
        printf("write check in child is %d\n", write_check);
        int close_check = fs_close(0);
        printf("close check top is %d\n", close_check);
        if (umount_fs(disk_name) == 0)
            printf("Unmount successful\n");
    }
    if (pid != 0) {
        wait(NULL);
        if (mount_fs("ab") == -1)
            return 0;
        int a = fs_open("fn");
        printf("open in parent = %d\n", a);

        char * read_buf = (char *) malloc(16000000);

        int read_check = fs_read(0, read_buf, 16000000);
        printf("read check is %d\n", read_check);

        int close_check = fs_close(0);
        printf("close check bottom is %d\n", close_check);
        if (umount_fs("ab") == 0)
            printf("Unmount successful\n");
    }
}

int main() {
    if (make_fs("ab") == -1)
        return -1;
    if (mount_fs("ab") == -1)
        return 0;

    char * file_name, * file_name2;
    file_name = (char *) calloc(17, sizeof(char));
    file_name2 = (char *) calloc(16, sizeof(char));
    file_name[0] = 'f';
    file_name[1] = 'n';
    file_name2[0] = 'f';
    file_name2[1] = 'n';
    file_name2[2] = '2';

    if (fs_create(file_name) == -1)
        return -3;
    if (fs_create(file_name2) == -1)
        return -3;
    int a = fs_open(file_name);
    printf("a = %d\n", a);
    a = fs_open(file_name2);
    printf("a is now %d\n", a);

    char * buf = (char *) malloc(1000*1000 * 17);
    char * buf2 = (char *) malloc(1024*1024 * 8);
    char * buf3 = (char *) malloc(1024*1024 * 17);
    char * buf4 = (char *) malloc(1024*1024 * 8);

    memset(buf, 'w', 1000*1000 * 17);
    memset(buf2, 'z', 1024*1024 * 8);

    for (int i = 0; i < 17; i++) {
        int write_check = fs_write(0, buf, 1000 * 1000 * 16);
        printf("write check = %d \n", write_check);
    }

    for (int i = 0; i < 4; i++) {
        int write_check = fs_write(1, buf2, 1024 * 1024 * 2);
        printf("write check 2 = %d \n", write_check);
    }


    fs_lseek(0, 0);
    fs_lseek(1, 0);
    int b = fs_read(0, buf3, 1000 * 1000 * 16);
    int count = 0;
    for (int i = 0; i < 1000 * 1000 * 17; i++) {
        if (buf[i] != buf3[i])
            count++;
    }
    printf("Discrepancy 1 is %d\n ", count);
    int c = fs_read(1, buf4, 1024*1024*8);




    count = 0;
    for (int i = 0; i < 1024 * 1024 * 8; i++) {
        if (buf2[i] != buf4[i])
            count++;
    }

    printf("Discrepancy 2 is %d\n ", count);

    printf("read = %d\n", b);
    printf("read = %d\n", c);

}

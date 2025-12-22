#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define FILE_NAME "test_file.txt"

int main () {
    printf("1. 화면에 출력 \n");
    
    int fd = open(FILE_NAME,O_WRONLY|O_CREAT|O_TRUNC, 0644);

    if(fd>0) {
        write(fd,"2.파일에 내용 기입\n",20);
        close(fd);
    }//end of if
    
    return 0;
}//end of main
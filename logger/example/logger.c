#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOG_FILE "C_log.txt"
#define LOG_TIME_FORMAT_SIZE 20

// 현 시간을 YYYY-MM-DD HH:MM:SS 포맷으로 변환
void get_timestamp(char *buffer, size_t size){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S",t);
}

// 로그파일 한줄 추가 
int append_log(const char *message) {
    FILE *fp = fopen(LOG_FILE,"a");
    if(fp==NULL) {
        perror("fopen");
        return -1;
    }

    char ts[LOG_TIME_FORMAT_SIZE]={0,};
    get_timestamp(ts,sizeof(ts));

    fprintf(fp,"[%s] %s\n",ts,message);
    fclose(fp);
    return 0;
}

//로그 파일 전체 출력 
int show_log(void) {
    FILE *fp = fopen(LOG_FILE, "r");
    if (fp ==NULL) {
        perror("fopen");
        fprintf(stderr, "로그파일 (%s)이 아직 없을 수 있습니다.\n",LOG_FILE);
        return -1;
    }

    int c;
    while((c=fgetc(fp))!=EOF) {
        putchar(c);
    }
    fclose(fp);
    return 0;
}

int main(int argc, char *argv[]) {
    //인자 없을 시 사용법 출력 
    if (argc <2) {
        printf("사용법: \n");
        printf(" %s 메세지내용...\n",argv[0]);
        printf(" %s --show #로그 전체 보기 \n",argv[0]);
        return 0;
    }

    // --show 옵션 처리
    if (strcmp(argv[1], "--show") == 0) {
        return show_log();
    }

    //그 외 메세지 합쳐서 하나의 문자열 만들기
    // argv[1]...[2]... [argc-1] 공백으로 이어붙이기

    size_t total_len=0;
    for(int i=1; i<argc; ++i) {
        total_len +=strlen(argv[i])+1; //공백 포함
    }

    char *message = (char*)malloc(total_len);

    if(message ==NULL) {
        fprintf(stderr, "메모리할당 실패\n");
        return -1;
    }

    //time stamp 이후 로그기록 시 한칸 띄우기
    message[0]='\0';

    for (int i=1; i<argc;++i) {
        strcat(message,argv[i]);
        if(i<argc-1){
            strcat(message," ");
        }
    }

    printf("show original message: %s\n",message);

    int result = append_log(message);
    free(message);

    if (result == 0 ) {
        printf("로그 기록 완료 (%s)\n",LOG_FILE);
    }

    return result == 0 ? 0:-1;
}
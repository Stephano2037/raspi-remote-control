#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../common.h"



//로그 레벨
typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
}LogLevel;



const char* level_to_string(LogLevel level) {
    switch (level) {
        case LOG_INFO: return "INFO";
        case LOG_WARN: return "WARN";
        case LOG_ERROR: return "ERROR";
        default: return "INFO";
    }
}


//logs 디렉토리 없을 경우 생성 
void create_log_dir(void) {
    struct stat st;
    if(stat(LOG_DIR,&st)==-1){
        //if dir not exist then create 
        if (mkdir (LOG_DIR,0755)==-1) {
            perror("mkdir logs");
        }
    }
}

//오늘 날짜 기준 로그 파일 경로 획득 : logs/YYYY-MM-DD.log
void get_today_log_path(char* buffer, size_t size){
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    char date[DATE_STRING_FORMAT_SIZE]={0,};

    strftime(date,sizeof(date),"%Y-%m-%d",t);

    snprintf(buffer,size, "%s/%s.log",LOG_DIR,date);
}

//timestamp 문자열: "YYYY-MM-DD HH:MM:SS"
void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer,size,"%Y-%m-%d %H:%M:%S",t);
}

//write log 
int append_log(LogLevel level, const char *message) {
    create_log_dir();

    char log_path[MAXBUFFER]={0,};
    get_today_log_path(log_path,sizeof(log_path));

    FILE *fp = fopen(log_path, "a");
    if (fp==NULL) {
        perror("fopen");
        return -1;
    }

    char ts[LOG_TIME_FORMAT_SIZE]={0,};
    get_timestamp(ts,sizeof(ts));

    fprintf(fp, "[%s] [%s] %s\n", ts, level_to_string(level),message);
    fclose(fp);

    printf("로그가 기록되었습니다. (%s)\n",log_path);

    return 0;
}//end of append_log


// 오늘 날짜에 기입된 로그들 출력
int show_today_log(void) {
    char log_path[MAXBUFFER] ={0,};
    get_today_log_path(log_path, sizeof(log_path));

    FILE *fp = fopen(log_path, "r");
    if (fp == NULL) {
        perror("fopen");
        fprintf(stderr, "오늘 날짜의 로그 파일(%s)이 아직 없을 수 있습니다.\n", log_path);
        return -1;
    }

    int c;
    while ((c = fgetc(fp)) != EOF) {
        putchar(c);
    }
    fclose(fp);
    return 0;
}

//문자열을 LogLevel 로 변환
LogLevel parse_level(const char *s) {
    if(strcasecmp(s,"INFO")==0) return LOG_INFO;
    if(strcasecmp(s,"WARN")==0) return LOG_WARN;
    if(strcasecmp(s,"ERROR")==0) return LOG_ERROR;

    return LOG_INFO;
}

void print_usage(const char *msg) {
    printf("사용법:\n");
    printf(" %s 메세지내용...\n" ,msg);
    printf(" %s -I LEVEL 메세지내용...\n",msg);
    printf("  LEVEL: INFO | WARN | ERROR\n");
    printf(" %s --show #오늘 날짜 로그 보기\n",msg);
}

int main(int argc, char *argv[]) {
    if (argc <2 ) {
        print_usage(argv[0]);
        return 0;
    }

    // --show 옵션
    if (strcmp(argv[1] , "--show")==0) {
        return show_today_log();
    }

    LogLevel level = LOG_INFO;
    int msg_start_index = 1;

    // -I Level 옵션 
    if (strcmp(argv[1], "-I") == 0 ) {
        if (argc <4) {
            print_usage(argv[0]);
            return 0;
        }
        level = parse_level(argv[2]);

        //
        msg_start_index = LOG_COMMAND_START_INDEX;
    }

    //전체 메세지 합치기 
    size_t total_len = 0;
    for(int i= msg_start_index; i<argc; ++i) {
        total_len +=strlen(argv[i])+1;
    }

    char * msg = (char*)malloc(total_len);
    if(msg == NULL) {
        fprintf(stderr, "메모리 할당 실패\n");
        return -1;
    }
    msg[0] = '\0';

    for (int i=msg_start_index; i<argc; ++i) {
        strcat(msg,argv[i]);
        if (i < argc - 1) {
            strcat (msg, " ");
        }//end of if
    } // end of for

    int result = append_log(level, msg);
    free(msg);

    return result == 0 ? 0: -1;

}
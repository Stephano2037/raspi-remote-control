#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

#define MAX_LOG_LENGTH 256 // 로그 메세지 최대 길이
#define QUEUE_SIZE 100 // 대기열 크기 

//자료구조 로그 큐
typedef struct {
    char msg[QUEUE_SIZE][MAX_LOG_LENGTH];
    int front; //꺼낼 위치
    int rear; // 삽입 위치
    int count;  // 현재 들어있는 개수 
    
    pthread_mutex_t lock; 
    pthread_cond_t not_empty; //큐가 비어있지 않음 공지 신호
    pthread_cond_t not_full; //큐가 다 차지 않음을 공지하는 신호 
}LogQueue;

LogQueue g_log_queue ={0,};
int g_is_running = 1;  //프로그램 종료 플래그

//큐 초기화
void init_queue() {
    g_log_queue.front = 0;
    g_log_queue.rear = 0;
    g_log_queue.count = 0;

    pthread_mutex_init(&g_log_queue.lock,NULL);
    pthread_cond_init(&g_log_queue.not_empty,NULL);
    pthread_cond_init(&g_log_queue.not_full,NULL);
}

// 생산자 - 로그 큐에 넣는 함수 
void async_log(const char* format,...) {
    pthread_mutex_lock(&g_log_queue.lock);

    //큐가 full -> 오래된 로그 버리기 or empty 선택 
    // 현재는 꽉차면 삭제 진행 
    if(g_log_queue.count >=QUEUE_SIZE) {
        printf("[WARN] Log Queue Full! Drop Message\n");
        pthread_mutex_unlock(&g_log_queue.lock);
        return; 
    }

    //메세지 포맷팅
    char buffer[MAX_LOG_LENGTH]={0,};
    va_list args;
    va_start(args,format);
    vsnprintf(buffer,sizeof(buffer),format,args);
    va_end(args);

    //큐에 복사
    strcpy(g_log_queue.msg[g_log_queue.rear],buffer);
    //환형 큐 이동진행
    g_log_queue.rear = (g_log_queue.rear+1)%QUEUE_SIZE;
    g_log_queue.count++;

    // 유휴 상태 로그 스레드 깨우기
    //lock -> signal -> unlock 패턴으로 진행해야함 
    // 왜냐하면 signal 보내도, MUTEX가 해제안되어있다면 스레드가 깨어나도 할수있는게 없음
    pthread_cond_signal(&g_log_queue.not_empty);
    pthread_mutex_unlock(&g_log_queue.lock);
} //end of async_log


//소비자 백그라운드 로그 스레드

void *consumer_logger_thread_func(void* arg) {
    FILE *fp = fopen("async_log.txt","a");
    if(!fp) {
        perror("fopen \n");
        return NULL;
    } //end of if 

    while(1) {
        pthread_mutex_lock(&g_log_queue.lock); 

        //큐는 비어있는데 프로그램이 실행중 인 경우 -> Wait
        // 생산자로부터 신호가 깨워지면 바로 inner while 아래줄부터 실행
        while(g_log_queue.count==0 && g_is_running) {
            // lock 해제 및 신호 오기까지 대기 
            // 신호 수신 -> lock 설정 후 wakeup
            // not_empty 가 설정되면 날 깨워주렴 이란 의미...(진동벨 눌렀는지 여부로 확인하면될듯)
            pthread_cond_wait(&g_log_queue.not_empty,&g_log_queue.lock);
        }//end of while

        // 종료 신호 왔으나 큐가 비어있으면 -> 스레드 종료 (루프 탈출)
        if( g_log_queue.count==0 && !g_is_running ) {
            pthread_mutex_unlock(&g_log_queue.lock);
            break;
        } //end of if

        char msg[MAX_LOG_LENGTH]={0,};
        strcpy(msg,g_log_queue.msg[g_log_queue.front]);
        g_log_queue.front = (g_log_queue.front+1) % QUEUE_SIZE;
        g_log_queue.count--;

        pthread_mutex_unlock(&g_log_queue.lock); //큐 조작 완료 후 lock 해제
    
        //실제 파일쓰기 ()
        fprintf(fp, "%s\n",msg);
        fflush(fp); // 즉시 기록 
        // printf("[DEBUG] Wrote: %s\n",msg);

    } // end of while
    
    fclose(fp);
    printf("Consumer Logger Thread Finished.\n");
    return NULL;
}//end of comsumer logger thread func

int main() {
    init_queue();
    pthread_t log_thread;
    pthread_create(&log_thread,NULL,consumer_logger_thread_func,NULL);

    printf("Main Start generating logs...\n");

    for(int i=0; i<QUEUE_SIZE; ++i) {
        async_log("[INFO] Log message number %d",i);
        usleep(10000); //매 10ms 마다 로그 쓰기 (100회)
    }//end of for

    printf("Main: Finished generating logs. Waiting for logger...\n");

    pthread_mutex_lock(&g_log_queue.lock);
    g_is_running = 0; //종료 플래그 설정
    pthread_cond_signal(&g_log_queue.not_empty);
    pthread_mutex_unlock(&g_log_queue.lock);

    // 로그 스레드에서 남은 큐 drop 대기 
    pthread_join(log_thread,NULL);

    printf("Main: ALL Done\n");
    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdarg.h>

//task 구조체 : 함수 포인터와 인자를 담음

typedef struct {
    void (*function)(void *); // execute function
    void *argument; // argument for function
}task_t;


// thread pool structure

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads; //스레드 id array;
    task_t *queue; //작업 큐 (원형 버퍼)
    int thread_count; 
    int queue_size; 
    int front; // take out
    int rear; //insert 
    int count; //현재 대기중인 작업 수
    int shutdown; //종료 플래그

}threadpool_t;

// working thread func (while loop)
void *thread_pool_worker(void* threadpool) {
    threadpool_t *pool = (threadpool_t*)threadpool; 

    while(1) {
        pthread_mutex_lock(&(pool->lock));

        // 일이 없고 종료 신호 없으면 대기 (sleep)
        while(pool->count == 0 && pool->shutdown==0) {
            pthread_cond_wait(&(pool->notify),&(pool->lock));
        }

        //종료 신호 오고 일감 없으면 스레드 종료 
        if(pool->shutdown == 1 && pool->count==0) {
            pthread_mutex_unlock(&(pool->lock));
            pthread_exit(NULL);
        }

        // 큐에서 일감 꺼내기 (POP) 
        task_t task={0,}; 
        task.function = pool->queue[pool->front].function;
        task.argument = pool->queue[pool->front].argument; 

        pool->front = (pool->front+1) % pool->queue_size;
        pool->count--;

        //unlock
        pthread_mutex_unlock(&(pool->lock));

        //실제 작업 수행
        (*(task.function))(task.argument);
    }//end of while
}

//init thread pool
threadpool_t *thread_pool_create(int thread_count, int queue_size) {
    threadpool_t *pool = (threadpool_t*)malloc(sizeof(threadpool_t));

    //init variable
    pool->thread_count = thread_count ; 
    pool->queue_size = queue_size; 
    pool->front = pool->rear = pool->count = 0;
    pool->shutdown = 0;

    pool->queue = (task_t*)malloc(sizeof(task_t)*queue_size);
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t)*thread_count);

    pthread_mutex_init(&(pool->lock),NULL);;
    pthread_cond_init(&(pool->notify),NULL);

    // create threads of workers
    for (int i=0;i<thread_count;++i) {
        pthread_create(&(pool->threads[i]),NULL,thread_pool_worker,(void*)pool);
    }

    return pool;

}// end of init thread pool func

//add task in thread pool

int thread_pool_add(threadpool_t* pool, void(*function)(void*),void *argument) {
    pthread_mutex_lock(&(pool->lock));

    //is Q full? 
    if(pool -> count == pool->queue_size || pool->shutdown) {
        pthread_mutex_unlock(&(pool->lock));
        return -1; //error
    } // return error right now

    pool->queue[pool->rear].function = function;
    pool->queue[pool->rear].argument = argument;
    pool->rear = (pool->rear+1) % pool->queue_size;
    pool->count++;

    //wake up idle worker
    pthread_cond_signal(&(pool->notify));

    pthread_mutex_unlock(&(pool->lock));

    return 0; 

}//end of pthread_pool add func

//thread pool finish and resource release
void thread_pool_destroy(threadpool_t *pool) {
    pthread_mutex_lock(&(pool->lock));
    pool->shutdown = 1; //finish flag setting
    pthread_cond_broadcast(&(pool->notify)); // wake up all workers and ready to release
    pthread_mutex_unlock(&(pool->lock));

    // waiting until all threads finished
    for (int i=0;i<pool->thread_count; ++i) {
        pthread_join(pool->threads[i], NULL);
    }

    //release memory
    free(pool->threads);
    free(pool->queue);
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));
    free(pool);
} //end of thread_pool_destroy func

// dummy function for example
void simulate_web_request(void *arg) {
    int request_id = *(int *)arg;
    
    // 스레드 ID 확인 (어떤 일꾼이 일하는지 보기 위함)
    pthread_t id = pthread_self();
    
    printf("[Worker %lu] request #%d processing starting... (DB searching)\n", (unsigned long)id, request_id);
    usleep(100 * 1000); // 0.1초 대기 (작업 시뮬레이션)
    printf("[Worker %lu] request #%d processing finisehd! 200 OK\n", (unsigned long)id, request_id);
    
    free(arg); // 인자 메모리 해제
}

int main() {
    printf("=== 스레드 풀 웹 서버 시뮬레이터 시작 ===\n");

    // create thread pool  (worker(threads) 4 , queue (waiting) 10 )
    threadpool_t *pool = thread_pool_create(4, 10);
    printf("스레드 풀 생성 완료 (Threads: 4)\n\n");

    // dummy request 
    for (int i = 0; i < 20; i++) {
        int *arg = (int *)malloc(sizeof(int));
        *arg = i + 1;
        
        // 작업을 풀에 던짐
        printf("메인 스레드: 요청 #%d 접수 중...\n", i + 1);
        while(thread_pool_add(pool, simulate_web_request, (void *)arg) < 0) {
            // 큐가 꽉 차면 잠시 대기 (Backpressure)
            printf(" [Warning] 큐가 꽉 찼습니다! 잠시 대기...\n");
            
            usleep(100000);     //100 ms 
        }
        
        // 요청이 너무 빨리 들어오는 상황 시뮬레이션       
        if (i % 5 == 0) usleep(50000);//50 ms
    }

    // waiting for real finish
    sleep(3); // 모든 작업이 끝날 때까지 여유를 둠
    
    printf("\n 서버 종료 요청...\n");
    thread_pool_destroy(pool);
    printf("=== 시뮬레이션 종료 ===\n");

    return 0;
}
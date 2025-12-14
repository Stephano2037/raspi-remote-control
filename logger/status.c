#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // for sleep()
#include <sys/statvfs.h> // for disk usage
#include <pthread.h>

#include "../common.h"

// 메모리 정보 (KB 단위)
typedef struct {
    long total;
    long free;
    long available;
    long buffers;
    long cached;
} MemInfo;

// CPU 시간 정보 (jiffies 단위)
typedef struct {
    long user;
    long nice;
    long system;
    long idle;
} CpuTimes;

#include <pthread.h>
#include <sys/statvfs.h> 


// define share data struct for result of all threads
typedef struct {
    float cpu_usage;
    
    long mem_used_mb;
    long mem_total_mb;
    float mem_percent;

    long disk_used_gb;
    long disk_total_gb;
    long disk_percent;

    float temp;
}SystemStatus;

// /proc/meminfo 파일 파싱해서 메모리 정보 가져오기
void *thread_mem_and_disk(void *arg) {
//int get_memory_info(MemInfo *mem) {
    SystemStatus *status = (SystemStatus*) arg;
    
    MemInfo mem={0,};  
    struct statvfs stat={0,};
   
    //1. memory
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        perror("fopen /proc/meminfo");
        status->mem_percent = -1.0f;
    }

    unsigned long total = 0, available = 0;

    char line[MAXBUFFER]={0,};
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %ld kB", &mem.total) == 1) continue;
        if (sscanf(line, "MemFree: %ld kB", &mem.free) == 1) continue;
        //mem available 이 없는 구형 커널도 추후 염두해야함
        if (sscanf(line, "MemAvailable: %ld kB", &mem.available) == 1) continue;
        if (sscanf(line, "Buffers: %ld kB", &mem.buffers) == 1) continue;
        if (sscanf(line, "Cached: %ld kB", &mem.cached) == 1) continue;
    }

    fclose(fp);

    // 메모리 정보 확인
    if (mem.total > 0) {
        status->mem_used_mb = (mem.total - mem.available) / 1024;
        status->mem_total_mb =  mem.total / 1024;
        status->mem_percent = 100.0f * ((float)(mem.total - mem.available)/(float)mem.total);
    } else {
        status->mem_percent = -1.0f;
    }
    
    //2. disk 
    if (statvfs("/", &stat) != 0) {
        perror("statvfs");
        return NULL;
    }

    // 64비트 정수로 계산  (오버플로 방지)
    unsigned long long total_bytes =
        (unsigned long long)stat.f_blocks * (unsigned long long)stat.f_frsize;
    unsigned long long free_bytes =
        (unsigned long long)stat.f_bfree  * (unsigned long long)stat.f_frsize;
    unsigned long long used_bytes = total_bytes - free_bytes;

    if (total_bytes > 0) {
        status->disk_total_gb = (long)(total_bytes / (1024ULL * 1024ULL * 1024ULL));
        status->disk_used_gb = (long)(used_bytes  / (1024ULL * 1024ULL * 1024ULL));
        status->disk_percent = 100.0f * ((float)used_bytes/ (float) total_bytes) ;
    }
    else {
        status->disk_percent = -1.0f;
    }

    return NULL;
}

// /proc/stat 파일 파싱해서 CPU 시간 정보 가져오기
int get_cpu_times(CpuTimes *cpu) {
    FILE *fp = fopen("/proc/stat", "r");
    if (fp == NULL) {
        perror("fopen /proc/stat");
        return -1;
    }

    char line[256];
    if (fgets(line, sizeof(line), fp)) {
        sscanf(line, "cpu %ld %ld %ld %ld", &cpu->user, &cpu->nice, &cpu->system, &cpu->idle);
    }
    
    fclose(fp);
    return 0;
}


// CPU 사용률 계산하기
void *thread_cpu(void *arg) {
    SystemStatus *status = (SystemStatus*) arg;

    CpuTimes prev={0,};
    CpuTimes curr={0,};

    if (get_cpu_times(&prev) != 0) {status->cpu_usage = -1.0; return NULL;}
    sleep(1); // 1초 대기
    if (get_cpu_times(&curr) != 0) {status->cpu_usage = -1.0; return NULL;}

    unsigned long long prev_idle = prev.idle;
    unsigned long long curr_idle = curr.idle;
    
    unsigned long long prev_total = prev.user + prev.nice + prev.system + prev.idle;
    unsigned long long curr_total = curr.user + curr.nice + curr.system + curr.idle;

    unsigned long long total_diff = curr_total - prev_total;
    unsigned long long idle_diff = curr_idle - prev_idle;

    if (total_diff == 0) status->cpu_usage=0.0f;
    else status->cpu_usage = 100.0f * (total_diff-idle_diff) / total_diff;

    return NULL;
}


// 루트(/) 파일 시스템의 디스크 사용량 가져오기
int get_disk_usage(float *percent, long *used_gb, long *total_gb) {
    //statvfs fields - 64 bit 
    struct statvfs stat={0,};
    if (statvfs("/", &stat) != 0) {
        perror("statvfs");
        return -1;
    }

    // 64비트 정수로 계산  (오버플로 방지)
    unsigned long long total_bytes =
        (unsigned long long)stat.f_blocks * (unsigned long long)stat.f_frsize;
    unsigned long long free_bytes =
        (unsigned long long)stat.f_bfree  * (unsigned long long)stat.f_frsize;
    unsigned long long used_bytes = total_bytes - free_bytes;

    if (total_bytes == 0) {
        return -1;
    }

    *total_gb = (long)(total_bytes / (1024ULL * 1024ULL * 1024ULL));
    *used_gb  = (long)(used_bytes  / (1024ULL * 1024ULL * 1024ULL));
    *percent  = 100.0f * (float)used_bytes / (float)total_bytes;

    return 0;
}


// 라즈베리파이 온도 가져오기
void* thread_temperature(void* arg) {
//float get_temperature(void) {
    // 라즈베리파이의 온도 센서 파일
    SystemStatus *status = (SystemStatus*) arg;

    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (fp == NULL) {
        // 이 파일이 없으면 일반 리눅스일 수 있으므로 에러 대신 -1 반환
        status->temp = -1.0f;
        return NULL;
    }

    int temp_milli_c= 0;
    fscanf(fp, "%d", &temp_milli_c);
    fclose(fp);
    
    status->temp = (float)temp_milli_c / 1000.0;
    return NULL;
}

int main() {
    printf("===== Raspberry Pi Status =====\n");

    pthread_t t1,t2,t3=0;
    SystemStatus status={0,}; 

    pthread_create(&t1,NULL,thread_cpu,&status);
    pthread_create(&t2,NULL,thread_mem_and_disk,&status);
    pthread_create(&t2,NULL,thread_temperature,&status);

    pthread_join(t1,NULL);
    pthread_join(t2,NULL);
    pthread_join(t3,NULL);
    
    // 1. CPU
    if(status.cpu_usage>0) printf("CPU Usage: %1.f%%\n",status.cpu_usage);
    else printf("CPU Usage: N/A\n");

    // 2. Memory
    if(status.mem_total_mb > 0) printf("Mem Usage: : %ld MB / %ld MB ( %1.f%%)\n",
    status.mem_used_mb , status.mem_total_mb, status.mem_percent);
    else printf("Mem Usage: N/A\n");
    
    // 3. Disk
    if(status.disk_total_gb > 0 ) printf("Disk (/)  : %ld GB / %ld GB (%.1f%%)\n", 
    status.disk_used_gb, status.disk_total_gb, status.disk_percent);
    else printf("Disk Usage: N/A");

    // 4. Temperature
    if (status.temp >= 0) {
        printf("Temp      : %.1f 'C\n", status.temp);
    } else {printf("temp N/A\n");}
    
    printf("===============================\n");

    return 0;
}


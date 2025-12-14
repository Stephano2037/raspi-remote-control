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
int get_memory_info(MemInfo *mem) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        perror("fopen /proc/meminfo");
        return -1;
    }

    char line[256]={0,};
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "MemTotal: %ld kB", &mem->total) == 1) continue;
        if (sscanf(line, "MemFree: %ld kB", &mem->free) == 1) continue;
        if (sscanf(line, "MemAvailable: %ld kB", &mem->available) == 1) continue;
        if (sscanf(line, "Buffers: %ld kB", &mem->buffers) == 1) continue;
        if (sscanf(line, "Cached: %ld kB", &mem->cached) == 1) continue;
    }

    fclose(fp);
    return 0;
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
float get_temperature(void) {
    // 라즈베리파이의 온도 센서 파일
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (fp == NULL) {
        // 이 파일이 없으면 일반 리눅스일 수 있으므로 에러 대신 -1 반환
        return -1.0f; 
    }

    int temp_milli_c;
    fscanf(fp, "%d", &temp_milli_c);
    fclose(fp);

    return (float)temp_milli_c / 1000.0;
}

int main() {
    printf("===== Raspberry Pi Status =====\n");

    pthread_t t1=0;
    SystemStatus status={0,}; 

    pthread_create(&t1,NULL,thread_cpu,&status);

    // 1. CPU
    pthread_join(t1,NULL);
    
    if(status.cpu_usage>0) printf("CPU Usage: %1.f%%\n",status.cpu_usage);
    else printf("CPU Usage: N/A\n");

    // float cpu_usage = calculate_cpu_usage();
    // if (cpu_usage >= 0) {
    //     printf("CPU Usage : %.1f%%\n", cpu_usage);
    // } else {
    //     printf("CPU Usage : N/A\n");
    // }

    // 2. Memory
    MemInfo mem;
    if (get_memory_info(&mem) == 0) {
        long used_mb = (mem.total - mem.available) / 1024;
        long total_mb = mem.total / 1024;
        float percent = 100.0f * used_mb / total_mb;
        printf("Memory    : %ld MB / %ld MB (%.1f%%)\n", used_mb, total_mb, percent);
    } else {
        printf("Memory    : N/A\n");
    }

    // 3. Disk
    float disk_percent=0.f;
    long used_gb = 0.f, total_gb=0.f;
    if (get_disk_usage(&disk_percent, &used_gb, &total_gb) == 0) {
        printf("Disk (/)  : %ld GB / %ld GB (%.1f%%)\n", used_gb, total_gb, disk_percent);
    } else {
        printf("Disk (/)  : N/A\n");
    }

    // 4. Temperature
    float temp = get_temperature();
    if (temp >= 0) {
        printf("Temp      : %.1f 'C\n", temp);
    } else {
        // 온도를 못 읽는 경우 (라즈베리파이가 아닐 때)는 출력 안 함
    }
    
    printf("===============================\n");

    return 0;
}


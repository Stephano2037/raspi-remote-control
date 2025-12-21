#!/bin/bash
# 예제용: home 디렉토리의 일부를 tar 로 압축 
TARGET_DIR="$HOME/projects"
BACKUP_DIR="$HOME/backups"
mkdir -p "$BACKUP_DIR"

TS=$(date +"%Y%m%d_%H%M%S")
ARCHIVE="$BACKUP_DIR/projects_backup_$TS.tar.gz"

tar -czf "$ARCHIVE" "$TARGET_DIR" 2> /tmp/backup_err.log

if [ $? -eq 0 ]; then
  echo "백업 완료: $ARCHIVE"
elsee
  echo "백업 중 오류 발생"
  cat /tmp/backup_err.log
fi
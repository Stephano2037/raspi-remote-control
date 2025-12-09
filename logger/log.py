import sys
from datetime import datetime
from pathlib import Path

LOG_FILE = Path("log.txt")

def write_log(message:str) -> None:
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    line=f"[{now}]{message}\n"
    with LOG_FILE.open("a",encoding="utf-8") as f:
        f.write(line)

def show_log() -> None:
    if not LOG_FILE.exists():
        print("로그 파일이 아직 없습니다.")
        return
    with LOG_FILE.open("r",encoding="utf-8") as f:
        print(f.read())

def main():
    if len(sys.argv)==1:
        print("사용법:")
        print(" python3 log.py 메세지내용...")
        print(" python3 log.py --show #로그전체보기 ")
        sys.exit(0)

    if sys.argv[1] =="--show":
        show_log()
    else:
        message=" ".join(sys.argv[1:])
        write_log(message)
        print("로그가 기록되었습니다.")

if __name__=="__main__":
    main()
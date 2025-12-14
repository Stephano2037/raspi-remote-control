#!/usr/bin/env python3
import subprocess
from flask import Flask, render_template_string, request, redirect, url_for

app = Flask(__name__)

HTML = """
<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <title>Raspberry Pi Control Panel</title>
  <style>
    body { font-family: sans-serif; background: #222; color: #eee; padding: 20px; }
    h1   { color: #6cf; }
    h2   { color: #9cf; }
    pre  { background: #111; padding: 15px; border-radius: 8px; white-space: pre-wrap; }
    .section { margin-bottom: 25px; }
    button { padding: 8px 16px; margin: 4px; }
    form { display: inline-block; }
  </style>
</head>
<body>
  <h1>Raspberry Pi Control Panel</h1>

  <div class="section">
    <h2>1. 시스템 상태</h2>
    <form method="post" action="{{ url_for('run_status_route') }}">
      <button type="submit">상태 새로고침</button>
    </form>
    {% if status_output %}
      <pre>{{ status_output }}</pre>
    {% endif %}
  </div>

  <div class="section">
    <h2>2. 로그 남기기 (logger_by_date)</h2>
    <form method="post" action="{{ url_for('log_route') }}">
      <input type="hidden" name="level" value="INFO">
      <button type="submit">INFO 로그 남기기</button>
    </form>
    <form method="post" action="{{ url_for('log_route') }}">
      <input type="hidden" name="level" value="WARN">
      <button type="submit">WARN 로그 남기기</button>
    </form>
    <form method="post" action="{{ url_for('log_route') }}">
      <input type="hidden" name="level" value="ERROR">
      <button type="submit">ERROR 로그 남기기</button>
    </form>

    <form method="post" action="{{ url_for('show_log_route') }}">
      <button type="submit">오늘 로그 보기</button>
    </form>

    {% if log_output %}
      <pre>{{ log_output }}</pre>
    {% endif %}
  </div>

  <div class="section">
    <h2>3. 백업 스크립트 실행 (backup.sh)</h2>
    <form method="post" action="{{ url_for('backup_route') }}">
      <button type="submit">백업 실행</button>
    </form>

    {% if backup_output %}
      <pre>{{ backup_output }}</pre>
    {% endif %}
  </div>
</body>
</html>
"""

def run_cmd(cmd):
    """
    공통: shell 명령 실행해서 stdout+stderr 문자열로 반환
    cmd는 리스트 형태 예: ["./status"]
    """
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True
        )
        output = result.stdout
        if result.stderr:
            output += "\n[stderr]\n" + result.stderr
        return output.strip()
    except FileNotFoundError:
        return f"명령을 찾을 수 없습니다: {' '.join(cmd)}"
    except Exception as e:
        return f"명령 실행 중 예외 발생: {e}"

@app.route("/", methods=["GET"])
def index():
    # 기본 GET 요청에서는 실행 결과 없이 빈 화면(버튼만) 보여주기
    return render_template_string(HTML,
                                  status_output=None,
                                  log_output=None,
                                  backup_output=None)

@app.route("/status", methods=["POST"])
def run_status_route():
    status_output = run_cmd(["./status"])
    return render_template_string(HTML,
                                  status_output=status_output,
                                  log_output=None,
                                  backup_output=None)

@app.route("/log", methods=["POST"])
def log_route():
    level = request.form.get("level", "INFO")
    message = f"웹에서 {level} 로그 버튼을 눌렀습니다."
    # ./logger_by_date -l LEVEL 메시지...
    log_output = run_cmd(["./logger_by_date", "-l", level, message])
    return render_template_string(HTML,
                                  status_output=None,
                                  log_output=log_output,
                                  backup_output=None)

@app.route("/log/show", methods=["POST"])
def show_log_route():
    # ./logger_by_date --show
    log_output = run_cmd(["./logger_by_date", "--show"])
    return render_template_string(HTML,
                                  status_output=None,
                                  log_output=log_output,
                                  backup_output=None)

@app.route("/backup", methods=["POST"])
def backup_route():
    backup_output = run_cmd(["./backup.sh"])
    return render_template_string(HTML,
                                  status_output=None,
                                  log_output=None,
                                  backup_output=backup_output)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=False)

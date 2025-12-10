#!/usr/bin/env python3
import subprocess
from flask import Flask, render_template_string

app = Flask(__name__)

# 아주 간단한 HTML 템플릿 (상태 출력만)
HTML = """
<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <title>Raspberry Pi Status</title>
  <style>
    body { font-family: sans-serif; background: #222; color: #eee; padding: 20px; }
    pre  { background: #111; padding: 15px; border-radius: 8px; }
    h1   { color: #6cf; }
    .refresh { margin-bottom: 10px; }
  </style>
</head>
<body>
  <h1>Raspberry Pi Status</h1>
  <div class="refresh">
    <form method="get" action="/">
      <button type="submit">새로고침</button>
    </form>
  </div>
  <pre>{{ status_output }}</pre>
</body>
</html>
"""

def run_status():
    """
    C로 만든 ./status 프로그램을 실행해서 stdout을 문자열로 반환
    """
    try:
        # stdout, stderr 둘 다 받고, 실패해도 예외 대신 결과를 보고 싶으면 check=False
        result = subprocess.run(
            ["./status"],
            capture_output=True,
            text=True
        )
        output = result.stdout
        if result.stderr:
            output += "\n[stderr]\n" + result.stderr
        return output
    except FileNotFoundError:
        return "./status 실행 파일을 찾을 수 없습니다. (컴파일/경로 확인)"
    except Exception as e:
        return f"./status 실행 중 예외 발생: {e}"

@app.route("/")
def index():
    status_output = run_status()
    return render_template_string(HTML, status_output=status_output)

if __name__ == "__main__":
    # 0.0.0.0: 외부(같은 네트워크)에서도 접속 가능
    app.run(host="0.0.0.0", port=5000, debug=False)

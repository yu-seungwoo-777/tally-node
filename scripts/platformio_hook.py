"""
PlatformIO Build Hook
빌드 전 web 폴더의 npm run deploy 실행
"""

Import("env", "projenv")

def pre_build_action(source, target, env):
    """빌드 전 실행: web UI 배포"""
    import subprocess
    import os

    web_dir = os.path.join(env["PROJECT_DIR"], "web")

    print("\n" + "=" * 60)
    print("Running: npm run deploy (web UI)")
    print("=" * 60)

    try:
        # npm run deploy 실행
        result = subprocess.run(
            ["npm", "run", "deploy"],
            cwd=web_dir,
            capture_output=True,
            text=True
        )

        if result.returncode == 0:
            print("Web UI deploy: SUCCESS")
            # 출력이 너무 길면 요약 (선택)
            if result.stdout:
                for line in result.stdout.splitlines()[-5:]:  # 마지막 5줄만
                    print("  ", line)
        else:
            print("Web UI deploy: FAILED")
            print(result.stderr)
            # 실패해도 계속 진행하려면 주석 해제
            # env.Exit(1)

    except FileNotFoundError:
        print("Warning: npm not found. Skipping web UI deploy.")
    except Exception as e:
        print(f"Warning: {e}")

    print("=" * 60 + "\n")

# 모든 타겟에 대해 실행 시도
env.AddPreAction(None, pre_build_action)

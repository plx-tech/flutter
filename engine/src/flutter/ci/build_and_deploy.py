import os
import os.path as path
import shutil
import subprocess
import json


def make_path(*components):
  return path.join(os.getcwd(), *components)


def get_engine_hash():
  os.chdir("./flutter")
  p = subprocess.Popen(["git", "rev-parse", "HEAD"], stdout=subprocess.PIPE)
  hash = p.stdout.readline().decode("utf-8")
  os.chdir("..")
  return hash[:-1]


ENGINE_HASH = get_engine_hash()
DEPLOY_PATH = make_path("deploy", ENGINE_HASH)


def execute_command(command):
  print(f"Executing command: '{command}'")
  exit_code = os.system(command)
  print(f"Command '{command}' executed with code {exit_code}")
  if exit_code != 0:
    raise SystemExit(f"Command {command} exited with code {exit_code}.")


def ninja(config, targets):
  execute_command(f"ninja -C out/{config} {' '.join(targets)}")


def gn(params):
  execute_command("./flutter/tools/gn --no-goma " + " ".join(params))


def check_cwd():
  cwd = os.getcwd()
  if not cwd.endswith("engine/src"):
    raise SystemExit("The script must run in the engine/src directory.")
  print("Script is running in engine/src")


def clean_deploy_directory():
  if path.exists(DEPLOY_PATH) and path.isdir(DEPLOY_PATH):
    shutil.rmtree(DEPLOY_PATH)
  execute_command(f"mkdir -p {DEPLOY_PATH}")


def set_use_prebuild_dart_sdk(v):
  os.environ["FLUTTER_PREBUILT_DART_SDK"] = str(v)


def build(recipe_path):
  if not os.path.exists(recipe_path):
    raise SystemExit(f"Recipe {recipe_path} not found.")
  recipe = {}
  with open(recipe_path, "r") as recipe_file:
    recipe = json.load(recipe_file)
  builds = recipe.get("builds", [])
  for build in builds:
    gn(build["gn"])
    ninja(build["ninja"]["config"], build["ninja"]["targets"])
    archives = build.get("archives", [])
    for archive in archives:
      base_path = archive["base_path"]
      for include_path in archive["include_paths"]:
        destination_path = make_path(DEPLOY_PATH, include_path.replace(base_path, ""))
        os.makedirs(os.path.dirname(destination_path), exist_ok=True)
        if os.path.isdir(include_path):
          shutil.copytree(include_path, destination_path, dirs_exist_ok=True)
        else:
          shutil.copyfile(include_path, destination_path)
  tasks = recipe.get("generators", {}).get("tasks", [])
  for task in tasks:
    language = task.get("language", "")
    command = language + " " + make_path(task["script"]) + " " + " ".join(task["parameters"])
    execute_command(command.strip())
  archives = recipe.get("archives", [])
  for archive in archives:
    source_path = make_path(archive["source"])
    destination_path = make_path(DEPLOY_PATH, archive["destination"])
    os.makedirs(os.path.dirname(destination_path), exist_ok=True)
    shutil.copyfile(source_path, destination_path)


def main():
  check_cwd()
  clean_deploy_directory()
  set_use_prebuild_dart_sdk(True)

  dart_bin = make_path("flutter", "third_party", "dart", "tools", "sdks", "dart-sdk", "bin")
  PATH = os.environ["PATH"]
  NEW_PATH = f"{dart_bin}:{PATH}"
  os.environ["PATH"] = NEW_PATH
  os.environ["ENGINE_CHECKOUT_PATH"] = os.path.dirname(os.getcwd())

  build("./flutter/ci/builders/clay_builders/host.json")
  build("./flutter/ci/builders/clay_builders/mac.json")
  build("./flutter/ci/builders/clay_builders/android_debug.json")
  build("./flutter/ci/builders/clay_builders/android_aot.json")
  build("./flutter/ci/builders/clay_builders/ios.json")
  build("./flutter/ci/builders/clay_builders/web.json")

  os.environ["PATH"] = PATH
  set_use_prebuild_dart_sdk(False)


if __name__ == "__main__":
  main()

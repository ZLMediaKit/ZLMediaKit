# -*- coding:utf-8 -*-
import copy
import os
import re
import subprocess
import sys
import json


def check_installed(command: str) -> bool:
    """
    check command is installed
    :param command:
    :return:
    """
    if os.system(f"command -v {command} > /dev/null") == 0:
        return True
    else:
        return False


def run_cmd(cmd: str, assert_success=False, capture_output=False, env=None) -> bool:
    """
    run cmd
    :param cmd:
    :param assert_success:
    :param env:
    :param capture_output:
    :param env:
    :return:
    """
    if not env:
        env = os.environ.copy()
    result = subprocess.run(cmd, shell=True, env=env, capture_output=capture_output)
    # Assert the command ran successfully
    if assert_success and result.returncode != 0:
        print("Command '" + cmd + "' failed with exit status code '" + str(
            result.returncode) + "'.\n\nExiting now.\nTry running the script again.")
        print(result.stderr.decode())
        sys.stdout.flush()
        sys.stderr.flush()
        sys.exit(1)
    return True


def check_dependencies() -> None:
    """
    check dependencies
    :return:
    """
    if not check_installed("p2o"):
        print()
        print("p2o is not installed, please install it first!")
        print("If you use npm, you can install it by the following command:")
        print("npm install -g postman-to-openapi")
        print()
        sys.exit(1)
    else:
        print("p2o is installed")


def get_version() -> str:
    """
    get version
    :return:
    """
    if os.path.isfile("../../cmake-build-debug/version.h"):
        print("Found version.h in cmake-build-debug")
        version_h_path = "../../cmake-build-debug/version.h"
    elif os.path.isfile("../../cmake-build-release/version.h"):
        print("Found version.h in cmake-build-release")
        version_h_path = "../../cmake-build-release/version.h"
    else:
        print("version.h not found")
        print("Please compile first")
        exit()
    with open(version_h_path, 'r') as f:
        content = f.read()
        commit_hash = re.search(r'define COMMIT_HASH (.*)', content).group(1)
        commit_time = re.search(r'define COMMIT_TIME (.*)', content).group(1)
        branch_name = re.search(r'define BRANCH_NAME (.*)', content).group(1)
        build_time = re.search(r'define BUILD_TIME (.*)', content).group(1)
        version = f"ZLMediaKit(git hash:{commit_hash}/{commit_time},branch:{branch_name},build time:{build_time})"
    print(f"version: {version}")
    return version


def get_secret() -> str:
    """
    get secret from default config file or user config file
    :return:
    """
    default_postman = json.load(open("../../postman/127.0.0.1.postman_environment.json", 'r'))
    secret = "035c73f7-bb6b-4889-a715-d9eb2d1925cc"
    for item in default_postman["values"]:
        if item["key"] == "ZLMediaKit_secret":
            secret = item["value"]
            break
    for root, dirs, files in os.walk("../../release/"):
        for file in files:
            if file == "config.ini":
                config_path = os.path.join(root, file)
                with open(config_path, 'r') as f:
                    content = f.read()
                    secret = re.search(r'secret=(.*)', content).group(1)
    return secret


def update_options(version: str, secret: str) -> None:
    """
    update options
    :param version:
    :param secret:
    :return:
    """
    print("update options")
    options = json.load(open("./options.json", 'r'))
    options["info"]["version"] = version
    options["additionalVars"]["ZLMediaKit_secret"] = secret
    json.dump(options, open("./options.json", 'w'), indent=4)


def generate() -> None:
    """
    generate
    :return:
    """
    print("generate")
    run_cmd("p2o ../../postman/ZLMediaKit.postman_collection.json -f ../../www/swagger/openapi.json -o ./options.json",
            True, True)
    openapi = json.load(open("../../www/swagger/openapi.json", 'r'))
    for path in openapi["paths"]:
        openapi["paths"][path]["post"] = copy.deepcopy(openapi["paths"][path]["get"])
        openapi["paths"][path]["post"]["tags"] = ["POST"]
    # save
    json.dump(openapi, open("../../www/swagger/openapi.json", 'w'), indent=4)
    print("generate success")


if __name__ == "__main__":
    check_dependencies()
    version = get_version()
    secret = get_secret()
    update_options(version, secret)
    generate()

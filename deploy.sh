#!/usr/bin/env bash

#
# deploy
#
# Upload and deploy Microvisor application code
#
# @author    Tony Smith
# @copyright 2022-23, KORE Wireless
# @version   1.8.4a
# @license   MIT
#

#set -x

# GLOBALS
app_dir=app
app_name=mv-mqtt-demo.bin
#------------^ APP SPECIFIC ^------------
cmake_path="${app_dir}/CMakeLists.txt"
build_number_path="${app_dir}/.build_number"
bin_path="build/${app_dir}/${app_name}"
application="dummy"
private_key_path=NONE
public_key_path=NONE
do_log=0
do_build=1
do_deploy=1
do_update=1
do_gen_keys=0
output_mode=text
mvplg_minor_min="3"
mvplg_patch_min="8"

# NOTE
# This script assumes the build directory is called 'build' and exists
# within the project directory. If it is used to build the app, this will
# be the case. You can pass in alternative target for the build product
# eg. './deploy.sh build_alt/app/my_app.zip'

# FROM 1.6.0 -- Trap ctrl-c
stty -echoctl
trap 'echo End of Line' SIGINT

# FUNCTIONS
show_help() {
    echo -e "Usage:\n"
    echo -e "  ./deploy.sh /optional/path/to/Microvisor/app.bin\n"
    echo -e "Options:\n"
    echo "  --application {path}  Set application to build. Default: dummy"
    echo "  --log / -l            After deployment, start log streaming. Default: no logging"
    echo "  --genkeys             Generate remote debugging keys"
    echo "  --publickey {path}    /path/to/remote/debugging/public/key.pem"
    echo "                        Must be a pre-generated file if you do not include --genkeys"
    echo "  --privatekey {path}   /path/to/remote/debugging/private/key.pem"
    echo "                        Must be a pre-generated file if you do not include --genkeys"
    echo "  --deploy / -d         Deploy without a build"
    echo "  --buildonly / -b      Build without deploying"
    echo "  --logonly             Start log streaming immediately; do not build or deploy"
    echo "  --output / -o {mode}  Log output mode: \`text\` or \`json\`"
    echo "  --help / -h           Show this help screen"
    echo
}

stream_log() {
    echo -e "\nLogging from ${MV_DEVICE_SID}..."
    if [[ ${output_mode} == "json" ]]; then
        twilio microvisor:logs:stream "${MV_DEVICE_SID}" -o=json | jq
    else
        twilio microvisor:logs:stream "${MV_DEVICE_SID}"
    fi
}

set_keys() {
    echo -e "Generating Remote Debugging keys..."
    # Check for passed directories
    if [[ -d "${private_key_path}" ]]; then
        private_key_path="${private_key_path}/debug_auth_prv_key.pem"
    fi

    if [[ -d "${public_key_path}" ]]; then
        public_key_path="${public_key_path}/debug_auth_pub_key.pem"
    fi

    # Generate the keys using the Twilio CLI Microvisor plugin
    if twilio microvisor:debug:generate_keypair --debug-auth-privkey="${private_key_path}" --debug-auth-pubkey="${public_key_path}" --force ; then
        echo "Private key written to ${private_key_path}"
        echo " Public key written to ${public_key_path}"
    else
        show_error_and_exit "Could not generate remote debugging keys"
    fi
}

show_error_and_exit() {
    echo "[ERROR] $1... exiting"
    exit 1
}

check_prereqs() {
    #1: Bash version 4+
    bv=$(/usr/bin/env bash --version | grep 'GNU bash' | awk {'print $4'} | cut -d. -f1)
    [[ ${bv} -lt 4 ]] && show_error_and_exit "This script requires Bash 4+"

    #2: required utilities
    prqs=(jq cmake twilio)
    for prq in "${prqs[@]}"; do
        which "${prq}" > /dev/null 2>&1 || show_error_and_exit "Required utility ${prq} not installed"
    done

    #3: credentials set
    [[ -z ${TWILIO_ACCOUNT_SID} || -z ${TWILIO_AUTH_TOKEN} ]] && show_error_and_exit "Twilio credentials not set as environment variables"

    #4: Microvisor plugin version
    result=$(twilio plugins | grep 'microvisor' | awk {'print $2'})
    minor=$(echo "$result" | cut -d. -f2)
    patch=$(echo "$result" | cut -d. -f3)
    [[ ${minor} -lt ${mvplg_minor_min} ]] && show_error_and_exit "Microvisor plugin 0.${mvplg_minor_min}.${mvplg_patch_min} or above required"
    [[ ${minor} -eq ${mvplg_minor_min} && ${patch} -lt ${mvplg_patch_min} ]] && show_error_and_exit "Microvisor plugin 0.${mvplg_minor_min}.${mvplg_patch_min} or above required"
}

build_app() {
    # Set up the build
    cmake -S . -B build -DAPPLICATION="$application"

    # Build the app itself
    if cmake --build build 2>&1 ; then
        echo "App built"
    else
        show_error_and_exit "Could not build the app"
    fi
}

update_build_number() {
    if [ -f ${build_number_path} ]; then
        old_num=$(<${build_number_path})
    else
        old_num=0
    fi

    ((new_num=old_num+1))
    echo "$new_num" >${build_number_path}
}

# RUNTIME START
# Check prequisites
check_prereqs

# Parse arguments
arg_is_value=0
last_arg=NONE
for arg in "$@"; do
    # Make arg lowercase
    check_arg=${arg,,}
    if [[ ${arg_is_value} -gt 0 ]]; then
        if [[ "${arg:0:1}" = "-" ]]; then
            show_error_and_exit "Missing value for ${last_arg}"
        fi
        case "${arg_is_value}" in
            1) application="${arg}" ;;
            2) private_key_path="${arg}" ;;
            3) public_key_path="${arg}"  ;;
            4) output_mode="${check_arg}" ;;
            *) echo "[Error] Unknown argument" exit 1 ;;
        esac
        arg_is_value=0
        continue
    fi

    if [[ "${check_arg}" = "--application" ]]; then
        arg_is_value=1
        last_arg=${arg}
        continue
    elif [[ "${check_arg}" = "--log" || "${check_arg}" = "-l" ]]; then
        do_log=1
    elif [[ "${check_arg}" = "--privatekey" ]]; then
        arg_is_value=2
        last_arg=${arg}
        continue
    elif [[ "${check_arg}" = "--publickey" ]]; then
        arg_is_value=3
        last_arg=${arg}
        continue
    elif [[ "${check_arg}" = "--output"  || "${check_arg}" = "-o" ]]; then
        arg_is_value=4
        last_arg=${arg}
        continue
    elif [[ "${check_arg}" = "--buildonly" || "${check_arg}" = "-b" ]]; then
        do_log=0
        do_deploy=0
        do_build=1
    elif [[ "${check_arg}" = "--logonly" ]]; then
        do_log=1
        do_deploy=0
        do_build=0
    elif [[ "${check_arg}" = "--genkeys" ]]; then
        do_gen_keys=1
    elif [[ "${check_arg}" = "--deploy" || "${check_arg}" = "-d" ]]; then
        do_build=0
    elif [[ "${check_arg}" = "--help" || "${check_arg}" = "-h" ]]; then
        show_help
        exit 0
    elif [[ "${arg:0:1}" = "-" ]]; then
        show_error_and_exit "Unknown command: ${arg}"
    else
        bin_path="${arg}"
    fi
done

# FROM 1.7.0 -- generate keys if required
if [[ ${do_gen_keys} -eq 1 ]]; then
    if [[ "${public_key_path}" = "NONE" ]]; then
        # Public key path not specified, so use the default
        public_key_path="$(pwd)/build/${app_dir}/debug_auth_pub_key.pem"
    fi

    if [[ "${private_key_path}" = "NONE" ]]; then
        # Private key path not specified, so use the default
        private_key_path="$(pwd)/build/${app_dir}/debug_auth_prv_key.pem"
    fi

    # Create the build directory so we can save keys there
    if [[ ! -e build ]]; then
        mkdir -p "$(pwd)/build/${app_dir}" || show_error_and_exit "Could not create the build directory"
    fi

    # Generate the keys
    set_keys
else
    # User didn't ask to generate keys - did they pass in a public key path?
    if [[ "${public_key_path}" != "NONE" ]]; then
        # Check key path
        if [[ ! -f "${public_key_path}" ]]; then
            show_error_and_exit "Public key cannot be found at ${public_key_path}"
        fi
    fi
fi

# FROM 1.6.0 -- check output mode
if [[ ${output_mode} != "text" ]]; then
    if [[ ${output_mode} != "json" ]]; then
        output_mode=text
    fi
fi

if [[ ${do_build} -eq 1 ]]; then
    [[ ${do_update} -eq 1 ]] && update_build_number
    build_app
fi

if [[ ${do_deploy} -eq 1 ]]; then
    # FROM 1.8.0 -- Bundle the .bin with the MV Plugin
    # Check we have what looks like a .bin
    extension="${bin_path##*.}"
    if [[ "${extension}" != "bin" ]]; then
        show_error_and_exit "${bin_path} does not indicate a .bin file"
    fi

    # Bundle the .bin
    zip_path="${bin_path%%.*}.zip"
    if [[ "${public_key_path}" != "NONE" ]]; then
        if ! twilio microvisor:apps:bundle "${bin_path}" "${zip_path}" --debug-auth-pubkey "${public_key_path}" 2>&1 ; then
            exit 1
        fi
    else
        if ! twilio microvisor:apps:bundle "${bin_path}" "${zip_path}" 2>&1 ; then
            exit 1
        fi
    fi

    # Try to upload the bundle
    echo "Uploading ${zip_path}..."
    upload_action=$(twilio microvisor:apps:create "${zip_path}" -o=json)
    app_sid=$(echo "${upload_action}" | jq -r '.[0].sid')

    if [[ -z "${app_sid}" || "${app_sid}" == "null" ]]; then
        show_error_and_exit "Could not upload app"
    else
        # Success... try to assign the app
        echo "Assigning app ${app_sid} to device ${MV_DEVICE_SID}..."
        if ! twilio api:microvisor:v1:devices:update --sid="${MV_DEVICE_SID}" --target-app="${app_sid}" > /dev/null 2>&1 ; then
            exit 1
        fi
    fi
fi

# Remove null file
rm -f null.d

# Dump remote debugging command if we can
if [[ "${public_key_path}" != "NONE" && "${private_key_path}" != "NONE" ]]; then
    echo -e "\nUse the following command to initiate remote debugging:"
    echo "twilio microvisor:debug ${MV_DEVICE_SID} '${private_key_path}'"
fi

# Start logging if requested to do so
[[ ${do_log} -eq 1 ]] && stream_log
exit 0

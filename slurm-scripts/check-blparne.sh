#!/bin/bash

USERNAME="blparne"

send_telegram() {
    	local message="$1"
        TOKEN=""
        CHATID=""

        curl -s -o /dev/null --config - <<EOF
url = "https://api.telegram.org/bot${TOKEN}/sendMessage"
data = "chat_id=${CHATID}"
data = "text=${message}"
EOF

        unset TOKEN
        unset CHATID
}


get_max_submit() {
    sacctmgr show assoc user="${USERNAME}" format=MaxSubmitJobs --noheader --parsable2 2>/dev/null | \
        head -1 | grep -E '^[0-9]+$' | tr -d ' '
}

send_telegram "Starting MaxSubmitJobs monitor..."

while true; do
    current=$(get_max_submit)
    if [[ -z "$current" ]]; then
        current=0
    fi
    if [[ "$current" -ge 1 ]]; then
        break
    fi
    sleep 60
done

send_telegram "You can submit jobs now (yippee!): MaxSubmitJobs is $(get_max_submit)"

exit 0


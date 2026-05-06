#!/bin/bash
while read -r user; do
    sacctmgr show assoc user="$user" format=user,account,MaxSubmitJobs --noheader --parsable2
done < users

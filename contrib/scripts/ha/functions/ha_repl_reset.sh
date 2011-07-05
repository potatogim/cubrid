#!/bin/bash

################################################################################
# 
prog_name='ha_repl_reset.sh'

log_home=
db_name=
host=
dba_password=

log_path=
now=$(date +"%Y%m%d_%H%M%S")
current_host=$(uname -n)
################################################################################

function print_usage()
{
	echo ""
	echo "Usage: $prog_name [options]"
	echo ""
	echo "    -l [log_home]"
	echo "    -d [db_name]"
	echo "    -h [host]"
	echo "    -p [dba_password]"
	echo ""
}

function is_invalid_args()
{
	[ -z "$log_home" ] && echo " << ERROR >> log_home is not specified" && return 0
	[ -z "$db_name" ] && echo " << ERROR >> db_name is not specified" && return 0
	[ -z "$host" ] && echo " << ERROR >> host is not specified" && return 0

	log_path=${log_home}/${db_name}_${host}
	new_log_path=${log_path}.${now}
	return 1
}

function is_utils_running()
{
	line=$(ps -ef | grep -v grep | grep "cub_admin copylogdb -L $log_path")
	args=$(echo $line | cut -d ' ' -f 8-)
	[ -n "$args" ] && echo " << ERROR >> $args process is now running" && return 0	

	line=$(ps -ef | grep -v grep | grep "cub_admin applylogdb -L $log_path")
	args=$(echo $line | cut -d ' ' -f 8-)
	[ -n "$args" ] && echo "<< ERROR >> $args process is now running" && return 0	

	return 1
}

function ha_repl_reset_copylogdb()
{
	echo -ne "\n\n1. remove old replication log on $current_host(master).\n\n"

	if [ -d "$log_path" ]; then
		echo " - move copied_log_path from $log_path to $new_log_path"

		echo "$current_host ]$ mv -f $log_path $new_log_path"
		mv -f $log_path $new_log_path

	fi

	echo -ne "\n - remove old replication log $log_path\n"

	echo "$current_host ]$ rm -rf $log_path"
	rm -rf $log_path
	echo "$current_host ]$ mkdir -p $log_path"
	mkdir -p $log_path
}

function ha_repl_reset_applylogdb()
{
	echo -ne "\n\n2. reset db_ha_apply_info.\n\n"
	if [ -z "$dba_password" ]; then

		echo " - move(backup) old db_ha_apply_info if exist"
		cmd_update="csql --sysadm -u dba -C $db_name@localhost -c \"UPDATE db_ha_apply_info SET copied_log_path='$new_log_path' WHERE db_name='$db_name' AND copied_log_path='$log_path'\""
		echo "$current_host ]$ $cmd_update"
		eval $cmd_update

		echo -ne "\n\n"
		echo " - select old db_ha_apply_info"
		cmd_select="csql -u dba -C $db_name@localhost -l -c \"SELECT db_name, db_creation_time, copied_log_path, page_id, offset FROM db_ha_apply_info WHERE db_name='$db_name' AND copied_log_path='$new_log_path'\"" 
		echo "$current_host ]$ $cmd_select"
		eval $cmd_select

	else
		echo " - move(backup) old db_ha_apply_info if exist"
		cmd_update="csql --sysadm -u dba -p '$dba_password' -C $db_name@localhost -c \"UPDATE db_ha_apply_info SET copied_log_path='$new_log_path' WHERE db_name='$db_name' AND copied_log_path='$log_path'\""
		echo "$current_host ]$ $cmd_update"
		eval $cmd_update

		echo -ne "\n\n"
		echo " - select old db_ha_apply_info"
		cmd_select="csql -u dba -p '$dba_password' -C $db_name@localhost -l -c \"SELECT db_name, db_creation_time, copied_log_path, page_id, offset FROM db_ha_apply_info WHERE db_name='$db_name' AND copied_log_path='$new_log_path'\"" 

		echo "$current_host ]$ $cmd_select"
		eval $cmd_select
	fi
}

function ha_repl_reset()
{
	ha_repl_reset_copylogdb
	ha_repl_reset_applylogdb
}


### main ##############################

while getopts "l:d:h:p:" optname
do
        case "$optname" in
                "l") log_home="${OPTARG}";;
                "d") db_name="${OPTARG}";;
                "h") host="${OPTARG}";;
                "p") dba_password="${OPTARG}";;
                "?") print_usage;;
                ":") print_usage;;
                *) print_usage;;
        esac
done

if is_invalid_args; then
	print_usage 
	exit 1
fi

if is_utils_running; then
	exit 1
fi

ha_repl_reset

sleep 1

exit 0


#!/bin/bash

#####################################################################
#  @author Rafal Slota
#  @copyright (C): 2014 ACK CYFRONET AGH
#  This software is released under the MIT license
#  cited in 'LICENSE.txt'.
#####################################################################
#  This script is used by Bamboo agent to set up VeilCluster nodes
#  during deployment.
#####################################################################

## Check configuration and set defaults...
if [[ -z "$CONFIG_PATH" ]]; then
    export CONFIG_PATH="/etc/onedata_platform.conf"
fi

if [[ -z "$SETUP_DIR" ]]; then
    export SETUP_DIR="/tmp/onedata"
fi

if [[ -z "$CREATE_USER_IN_DB" ]]; then
    export CREATE_USER_IN_DB="true"
fi

# Load funcion defs
source ./functions.sh || exit 1


########## Load Platform config ############
info "Fetching platform configuration from $MASTER:$CONFIG_PATH ..."
scp $MASTER:$CONFIG_PATH ./conf.sh || error "Cannot fetch platform config file."
source ./conf.sh || error "Cannot find platform config file. Please try again (redeploy)."


########## Install Script Start ############
ALL_NODES="$CLUSTER_NODES ; $CLUSTER_DB_NODES"
ALL_NODES=`echo $ALL_NODES | tr ";" "\n" | sed -e 's/^ *//g' -e 's/ *$//g' | sort | uniq`

n_count=`len "$ALL_NODES"`
for node in $ALL_NODES; do
    [[ 
        "$node" != ""
    ]] || continue
    
    install_rpm $node veilcluster.rpm
done


########## SetupDB Script Start ############
n_count=`len "$CLUSTER_DB_NODES"`
for i in `seq 1 $n_count`; do
    node=`nth "$CLUSTER_DB_NODES" $i`
    
    [[ 
        "$node" != ""
    ]] || error "Invalid node configuration !"
    
    start_db "$node" $i $n_count
    deploy_stamp "$node"
done


########## SetupCluster Script Start ############
if [[ `len "$CLUSTER_DB_NODES"` == 0 ]]; then
    error "There are no configured DB Nodes !"
fi

n_count=`len "$CLUSTER_NODES"`
for i in `seq 1 $n_count`; do
    node=`nth "$CLUSTER_NODES" $i`

    [[ 
        "$node" != ""
    ]] || error "Invalid node configuration !"
    
    start_cluster "$node" $i $n_count
    deploy_stamp "$node"
    sleep 10
done
sleep 120


########## SetupValidation Script Start ############
info "Setup Validation starts ..."

n_count=`len "$CLUSTER_NODES"`
for i in `seq 1 $n_count`; do
    
    node=`nth "$CLUSTER_NODES" $i`

    [[ 
        "$node" != ""
    ]] || error "Invalid node configuration !"

    pcount=`ssh $node "ps aux | grep beam | wc -l"`

    [[
        $pcount -ge 2
    ]] || error "Could not find cluster processes on the node !"
    
done


########## Nagios health check ############
cluster=`nth "$CLUSTER_NODES" 1`
cluster=${cluster#*@}

curl -k -X GET https://$cluster/nagios > hc.xml || error "Cannot get Nagios status from node '$cluster'"
stat=`cat hc.xml | sed -e 's/>/>\n/g' | grep -v "status=\"ok\"" | grep status`
[[ "$stat" == "" ]] || error "Cluster HealthCheck failed: \n$stat"


########## RegisterUsers Script Start ############
cnode=`nth "$CLUSTER_NODES" 1`
scp reg_user.erl $cnode:/tmp
escript_bin=`ssh $cnode "find /opt/veil/files/veil_cluster_node/ -name escript | head -1"` 
reg_run="$escript_bin /tmp/reg_user.erl"

n_count=`len "$CLIENT_NODES"`
for i in `seq 1 $n_count`; do
    
    node=`nth "$CLIENT_NODES" $i`
    node_name=`node_name $cnode`
    cert=`nth "$CLIENT_CERTS" $i`

    [[ 
        "$node" != ""
    ]] || error "Invalid node configuration !"

    [[ 
        "$cert" != ""
    ]] || continue

    user_name=${node%%@*}
    if [[ "$user_name" == "" ]]; then
        user_name="root"
    fi

    ## Add user to all cluster nodes
    n_count=`len "$CLUSTER_NODES"`
    for ci in `seq 1 $n_count`; do
        lcnode=`nth "$CLUSTER_NODES" $ci`
	ssh $lcnode "useradd $user_name 2> /dev/null || exit 0"
    done

    if [[ "$CREATE_USER_IN_DB" == "true" ]]; then
        cmm="$reg_run $node_name $user_name '$user_name@test.com' /tmp/tmp_cert.pem"

        info "Trying to register $user_name using cluster node $cnode (command: $cmm)"

        scp $cert tmp_cert.pem
        scp tmp_cert.pem $cnode:/tmp/

        ssh $cnode "$cmm"
    fi

done

exit 0




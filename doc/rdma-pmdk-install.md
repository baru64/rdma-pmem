## PMDK
<https://docs.pmem.io/persistent-memory/getting-started-guide/installing-pmdk/compiling-pmdk-from-source>

    yum install ndctl ndctl-devel ndctl-libs
    yum -y install epel-release
    yum install autoconf automake pkgconfig glib2-devel libfabric-devel pandoc ncurses-devel libfabric gcc gcc-c++ git daxctl-devel
    git clone https://github.com/pmem/pmdk
    cd pmdk
    make
    make install

## RDMA - Soft-RoCE - CentOS 7
<https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/7/html/networking_guide/sec-configuring_soft-_roce>

    # install packages
    yum install rdma-core rdma-core-devel infiniband-diags libibcommon libibmad libibverbs libibverbs-utils
    # add interface
    rxe_cfg start
    rxe_cfg add ens3
    rxe_cfg status
    ibv_devices
    # centos firewall
    firewall-cmd --zone=public --add-port=4791/udp --permanent
    firewall-cmd --zone=public --add-port=4791/tcp --permanent
    firewall-cmd --reload
    # test connection
    ibv_rc_pingpong -d rxe0 -g 0 # server
    ibv_rc_pingpong -d rxe0 -g 0 192.168.100.101 # client

## RDMA - Soft-RoCE - Fedora 33

    # install packages
    dnf install rdma-core rdma-core-devel infiniband-diags libibcommon libibmad libibverbs libibverbs-utils
    # add interface
    rdma link add rxe0 type rxe netdev eth1
    ibv_devices
    # firewall
    firewall-cmd --zone=public --add-port=18515/udp --permanent
    firewall-cmd --zone=public --add-port=18515/tcp --permanent
    firewall-cmd --zone=public --add-port=4791/udp --permanent
    firewall-cmd --zone=public --add-port=4791/tcp --permanent
    firewall-cmd --reload
    # test connection
    ibv_rc_pingpong -d rocep0s8 -g 0 # server
    ibv_rc_pingpong -d rocep0s8 -g 0 192.168.100.101 # client

#!/bin/bash
# add interface
sudo rdma link add rxe0 type rxe netdev eth1
ibv_devices

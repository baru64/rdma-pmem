#!/bin/bash
# firewall
sudo firewall-cmd --zone=public --add-port=18515/udp --permanent
sudo firewall-cmd --zone=public --add-port=18515/tcp --permanent
sudo firewall-cmd --zone=public --add-port=4791/udp --permanent
sudo firewall-cmd --zone=public --add-port=4791/tcp --permanent
sudo firewall-cmd --reload

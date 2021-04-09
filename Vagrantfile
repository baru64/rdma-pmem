# -*- mode: ruby -*-
# vi: set ft=ruby :
Vagrant.configure("2") do |config|

  config.vm.define "node1" do |node1|
    node1.vm.box = "roboxes/fedora33"
    node1.vm.box_version = "3.1.20"
    node1.vm.network "private_network", ip: "192.168.33.10"
    node1.vm.synced_folder ".", "/home/vagrant/host"

    node1.vm.provider "virtualbox" do |vb|
      vb.memory = "6144"
      vb.cpus = 2
    end
  end

  config.vm.define "node2" do |node2|
    node2.vm.box = "roboxes/fedora33"
    node2.vm.box_version = "3.1.20"
    node2.vm.network "private_network", ip: "192.168.33.11"
    node2.vm.synced_folder ".", "/home/vagrant/host"

    node2.vm.provider "virtualbox" do |vb|
      vb.memory = "6144"
      vb.cpus = 2
    end
  end

end

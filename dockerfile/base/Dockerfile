FROM ubuntu:22.04
RUN mkdir -p /cefore
WORKDIR /cefore
RUN apt update
RUN apt install -y git build-essential libssl-dev
RUN apt install -y automake
RUN apt install -y iputils-ping net-tools ifstat
RUN apt install -y nano
RUN apt install -y python3 iproute2
RUN apt -y clean
RUN git clone https://github.com/Ryota-netlab/RI_.git
RUN git clone https://github.com/cefore/cefpyco.git
WORKDIR /cefore/cefpyco
RUN apt install -y cmake python3-pip
RUN apt -y clean
RUN pip3 install --upgrade pip
RUN pip3 install setuptools click numpy py-solc-x web3
WORKDIR /cefore/runner_test
RUN apt -y clean

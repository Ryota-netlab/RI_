FROM ri_cefore/base
WORKDIR /cefore/cefore
ENV USER=root
RUN apt-get update && apt-get install -y openssl libssl-dev
# RUN ./configure --enable-cache
RUN ./configure --enable-cache
RUN make
RUN make install
RUN make clean
RUN ldconfig
RUN echo "CS_MODE=1" > /usr/local/cefore/cefnetd.conf
WORKDIR /cefore/cefpyco
ENV USER=root
RUN chmod -R 777 /cefore
RUN apt install -y cmake python3-pip
RUN apt -y clean
RUN pip3 install --upgrade pip
RUN pip3 install setuptools click numpy py-solc-x cryptography
RUN pip3 install build
RUN pip install py-solc-x
RUN pip install --no-cache-dir web3==5.31.1
RUN cmake .
RUN make
RUN make install
COPY ./entrypoint.bash /cefore
RUN chmod +x /cefore/entrypoint.bash
ENTRYPOINT ["/cefore/entrypoint.bash"]

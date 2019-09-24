FROM haoxiangpeng/latest-cpp

COPY . /usr/src/pibench
WORKDIR /usr/src/pibench
RUN mkdir build_tmp && cd build_tmp && cmake -DCMAKE_BUILD_TYPE=Release .. && make
ENTRYPOINT ["make", "-C", "build_tmp", "test" ]

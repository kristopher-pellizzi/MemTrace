FROM ubuntu:focal
ENV TZ=Europe/Rome
ENV DEPS="ninja-build libglib2.0-dev make gcc g++ pkg-config python3 python3-pip git"
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
WORKDIR /opt/MemTraceThesis
COPY . .
RUN apt-get -y update && apt-get -y install $DEPS && git checkout development && python3 -m pip install -r requirements.txt && make
ENV PATH="/opt/MemTraceThesis/bin:${PATH}"
WORKDIR /home
CMD ["/bin/bash"]
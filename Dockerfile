FROM ubuntu:focal
ENV TZ=Europe/Rome
ENV DEPS="ninja-build libglib2.0-dev make gcc g++ pkg-config python3 python3-pip git wget gettext"
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
WORKDIR /opt/MemTraceThesis
COPY . .
RUN apt-get -y update && apt-get -y install $DEPS && git checkout development && python3 -m pip install -r requirements.txt && make 
ENV PATH="/opt/MemTraceThesis/bin:${PATH}"
ENV AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1
ENV AFL_SKIP_CPUFREQ=1
ENV FORCE_UNSAFE_CONFIGURE=1
WORKDIR /home
CMD ["/bin/bash"]

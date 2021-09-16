FROM ubuntu:focal
ARG TZ=Europe/Rome
ARG DEPS="ninja-build libglib2.0-dev make gcc g++ pkg-config python3 python3-pip git wget gettext locales"
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
WORKDIR /opt/MemTraceThesis
COPY . .
RUN apt-get -y update && apt-get -y install $DEPS && git checkout development && python3 -m pip install -r requirements.txt && make
ENV PATH="/opt/MemTraceThesis/bin:${PATH}" \
    AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
    AFL_SKIP_CPUFREQ=1 \
    FORCE_UNSAFE_CONFIGURE=1 \
    LANG=it_IT.UTF-8 \
    LANGUAGE=it
WORKDIR /home
CMD ["/bin/bash"]

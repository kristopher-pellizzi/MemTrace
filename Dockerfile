FROM ubuntu:focal
ARG TZ=Europe/Rome
ARG DEPS="ninja-build libglib2.0-dev make gcc g++ pkg-config python3 python3-pip git wget gettext locales"
ENV LANG=it_IT.UTF-8 \
    LANGUAGE=it \
    LC_CTYPE="it_IT.UTF-8" \
    LC_NUMERIC="it_IT.UTF-8" \
    LC_TIME="it_IT.UTF-8" \
    LC_COLLATE="it_IT.UTF-8" \
    LC_MONETARY="it_IT.UTF-8" \
    LC_MESSAGES="it_IT.UTF-8" \
    LC_PAPER="it_IT.UTF-8" \
    LC_NAME="it_IT.UTF-8" \
    LC_ADDRESS="it_IT.UTF-8" \
    LC_TELEPHONE="it_IT.UTF-8" \
    LC_MEASUREMENT="it_IT.UTF-8" \
    LC_IDENTIFICATION="it_IT.UTF-8" \
    LC_ALL=""
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
WORKDIR /opt/MemTraceThesis
COPY . .
RUN apt-get -y update && apt-get -y install $DEPS && git checkout development && python3 -m pip install -r requirements.txt && make
ENV PATH="/opt/MemTraceThesis/bin:${PATH}" \
    AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 \
    AFL_SKIP_CPUFREQ=1 \
    FORCE_UNSAFE_CONFIGURE=1
WORKDIR /home
CMD ["/bin/bash"]

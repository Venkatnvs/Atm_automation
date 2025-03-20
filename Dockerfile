# Use an official Python image
FROM python:3.10

# Enable BuildKit for secure secret handling
ENV DOCKER_BUILDKIT=1

# Set the working directory
WORKDIR /app

# Install system dependencies required for dlib
RUN apt update
RUN apt-get install -y --fix-missing \
    build-essential \
    cmake \
    gfortran \
    git \
    wget \
    curl \
    graphicsmagick \
    libgraphicsmagick1-dev \
    libatlas-base-dev \
    libavcodec-dev \
    libavformat-dev \
    libgtk2.0-dev \
    libjpeg-dev \
    liblapack-dev \
    libswscale-dev \
    pkg-config \
    python3-dev \
    python3-numpy \
    software-properties-common \
    zip \
    && apt-get clean && rm -rf /tmp/* /var/tmp/*

RUN cd ~ && \
    mkdir -p dlib && \
    git clone -b 'v19.9' --single-branch https://github.com/davisking/dlib.git dlib/ && \
    cd  dlib/ && \
    python3 setup.py install --yes USE_AVX_INSTRUCTIONS

# Copy only requirements to leverage Docker caching
COPY requirements.txt .

# Install Python dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Copy application files (excluding secrets)
COPY . .

# Securely mount secrets (environment file & Firebase JSON)
RUN --mount=type=secret,id=_env,dst=/etc/secrets/.env cat /etc/secrets/.env
RUN --mount=type=secret,id=atm-54854-firebase-adminsdk-fbsvc-846f36282a,dst=/etc/secrets/atm-54854-firebase-adminsdk-fbsvc-846f36282a.json cat /etc/secrets/atm-54854-firebase-adminsdk-fbsvc-846f36282a.json

# Expose the port Flask will run on
EXPOSE 5000

# Start the Flask app with SocketIO support using Gunicorn
CMD ["gunicorn", "-k", "geventwebsocket.gunicorn.workers.GeventWebSocketWorker", "-w", "1", "app:app", "--bind", "0.0.0.0:5000"]
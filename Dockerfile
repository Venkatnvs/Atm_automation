# Use an official Python image
FROM python:3.10

# Enable BuildKit for secure secret handling
ENV DOCKER_BUILDKIT=1

# Set the working directory
WORKDIR /app

# Install system dependencies required for dlib
RUN apt update && apt install -y \
    cmake \
    build-essential \
    libboost-all-dev \
    libopenblas-dev \
    liblapack-dev \
    libx11-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy only requirements to leverage Docker caching
COPY requirements.txt .

# Install Python dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Copy application files (excluding secrets)
COPY . .

# Securely mount secrets (environment file & Firebase JSON)
RUN --mount=type=secret,id=_env,dst=/etc/secrets/.env cat /etc/secrets/.env
RUN --mount=type=secret,id=firebase,dst=/etc/secrets/atm-54854-firebase-adminsdk-fbsvc-846f36282a.json cat /etc/secrets/atm-54854-firebase-adminsdk-fbsvc-846f36282a.json

# Expose the port Flask will run on
EXPOSE 5000

# Start the Flask app with SocketIO support using Gunicorn
CMD ["gunicorn", "-k", "geventwebsocket.gunicorn.workers.GeventWebSocketWorker", "-w", "1", "app:app", "--bind", "0.0.0.0:5000"]
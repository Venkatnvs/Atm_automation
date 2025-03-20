# Use an official Python image
FROM python:3.10

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

# Copy requirements and install dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy application files
COPY . .

COPY .env /app/.env
COPY atm-54854-firebase-adminsdk-fbsvc-846f36282a.json /app/atm-54854-firebase-adminsdk-fbsvc-846f36282a.json

# Set environment variables using `source` (for Docker Compose use)
RUN export $(grep -v '^#' /app/.env | xargs)

# Expose the port Flask will run on
EXPOSE 5000

# Start the Flask app with SocketIO support using Gunicorn
CMD ["gunicorn", "-k", "geventwebsocket.gunicorn.workers.GeventWebSocketWorker", "-w", "1", "app:app", "--bind", "0.0.0.0:5000"]

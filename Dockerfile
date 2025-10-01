# ---- base builder ----
FROM python:3.12-slim AS builder
ENV PIP_DISABLE_PIP_VERSION_CHECK=1 \
    PIP_NO_CACHE_DIR=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

# System deps (add any you need, e.g., libpq-dev for Postgres)
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential curl ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy only dependency files first to leverage Docker layer caching
COPY requirements.txt .
RUN python -m pip install --upgrade pip \
 && pip wheel --wheel-dir=/wheels -r requirements.txt

# ---- runtime ----
FROM python:3.12-slim
ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

# Create a non-root user
RUN useradd -u 10001 -m appuser

WORKDIR /app

# Install runtime libs and CA certs (keeps image minimal)
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# Bring in wheels and source, then install
COPY --from=builder /wheels /wheels
COPY . /app
RUN pip install --no-cache-dir --no-index --find-links=/wheels -r requirements.txt

# Drop privileges
USER appuser

# Uvicorn on 0.0.0.0:8080 (change mdm.app:app to your module:app)
EXPOSE 8080
HEALTHCHECK --interval=30s --timeout=3s --retries=5 \
  CMD python -c "import socket; s=socket.socket(); s.settimeout(2); s.connect(('127.0.0.1',8080)); s.close()"

CMD ["uvicorn", "mdm.app:app", "--host", "0.0.0.0", "--port", "8080"]

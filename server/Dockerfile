# FRASIL River's service (README.md). schema.json: what the firmware's code writes, made by make_schema.py from the
# simulator before this build (deploy.sh runs it); the build stops if it isn't there.
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY app.py auth.py guard.py mail.py scenes.py shards.py store.py schema.json ./
# Non-root; a new named volume at /data inherits this ownership.
RUN useradd --system --uid 10001 --no-create-home river && mkdir -p /data && chown river /data
USER river
ENV RIVER_DB=/data/river.sqlite PYTHONUNBUFFERED=1
VOLUME ["/data"]
EXPOSE 8080
HEALTHCHECK --interval=60s --timeout=5s \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8080/api/health', timeout=4)"
# One worker: the rate limits live in its memory (guard.py).
CMD ["uvicorn", "app:create_app", "--factory", "--host", "0.0.0.0", "--port", "8080", "--no-server-header"]

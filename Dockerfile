# Use a lightweight Linux base with GCC installed
FROM gcc:latest

# Install OpenSSL dependencies (Required for your HTTPS calls)
RUN apt-get update && \
    apt-get install -y libssl-dev

# Set the working directory inside the container
WORKDIR /usr/src/app

# Copy all your files (server.cpp, index.html, headers) into the container
COPY . .

# Compile the C++ code
# Note: We add -lssl -lcrypto for OpenSSL and -pthread for threading
RUN g++ -o server server.cpp -lssl -lcrypto -pthread

# Expose port 8080 (Matches your code)
EXPOSE 8080

# Command to run the app when the server starts
CMD ["./server"]
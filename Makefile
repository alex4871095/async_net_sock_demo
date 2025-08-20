all: async_net_sock_demo
async_net_sock_demo:
	gcc mediator.c -o mediator
	gcc udp_client.c -o udp_client
	gcc tcp_server.c -o tcp_server


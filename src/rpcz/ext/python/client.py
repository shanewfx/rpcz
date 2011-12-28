#!/usr/bin/env python

import zmq
import search_pb2
import rpcz_pb2

def main():
    context = zmq.Context(1)
    client = context.socket(zmq.REQ);
    client.connect('tcp://localhost:5555')
    generic_request = rpcz_pb2.GenericRPCRequest()
    generic_request.service = 'rpcz.SearchService'
    generic_request.method = 'Search'
    client.send(generic_request.SerializeToString())
    generic_response = rpcz_pb2.GenericRPCResponse()
    generic_response.ParseFromString(client.recv())
    payload = search_pb2.SearchResponse()
    payload.ParseFromString(generic_response.payload)
    print payload

if __name__ == "__main__":
    main()
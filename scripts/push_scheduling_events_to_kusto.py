#!/usr/bin/python

import json
import urllib2
import socket
import sys
import pandas
import uuid 
import datetime


from azure.kusto.data.request import KustoConnectionStringBuilder
from azure.kusto.ingest import (
    KustoIngestClient,
    IngestionProperties,
    FileDescriptor,
    BlobDescriptor,
    StreamDescriptor,
    DataFormat,
    ReportLevel,
    KustoStreamingIngestClient,
)


cluster = "https://ingest-azurehn.kusto.windows.net"
kcsb = KustoConnectionStringBuilder.with_aad_device_authentication(cluster)
client = KustoIngestClient(kcsb)
ingestion_props = IngestionProperties(
    database="Azurehn",  #name of the database in KUSTO
    table="CPU_EVENTS",  #Name of table IN KUSTO
    dataFormat=DataFormat.csv,
    # incase status update for success are also required
    # reportLevel=ReportLevel.FailuresAndSuccesses,
)



metadata_url = "http://169.254.169.254/metadata/scheduledevents?api-version=2017-11-01"
headers = "{Metadata:true}"
this_host = socket.gethostname()

def get_scheduled_events():
   req = urllib2.Request(metadata_url)
   req.add_header('Metadata', 'true')
   resp = urllib2.urlopen(req)
   data = json.loads(resp.read())
   return data


def handle_scheduled_events(data):
    id=uuid.UUID(int=uuid.getnode())
    current_time=datetime.datetime.utcnow()
    for evt in data['Events']:
        eventid = evt['EventId']
        status = evt['EventStatus']
        resources = evt['Resources']
        eventtype = evt['EventType']
        resourcetype = evt['ResourceType']
        notbefore = evt['NotBefore'].replace(" ","_")
        fields =["VM_ID", "TIMESTAMP","EVENT_id","EVENT_TYPE","RESOURCES_TYPE","RESOURCES","EVENT_STATUS","NOTBEFORE"]
        rows = [[id, current_time,eventid,eventtype,resourcetype,resources,status,notbefore]]
        df = pandas.DataFrame(data=rows, columns=fields)
        client.ingest_from_dataframe(df, ingestion_properties=ingestion_props) # pushing all data related to events itno kusto
        if this_host in resources:
            print "+ Scheduled Event. This host " + this_host + " is scheduled for " + eventtype + " not before " + notbefore

           
   


def main():
   data = get_scheduled_events() #Collecting all scheduling event that are present 
   id=uuid.UUID(int=uuid.getnode()) # unique ID for VM
   current_time=datetime.datetime.utcnow() # current time
   if(len(data['Events'])==0): # if  their are no event push empty values into kusto
     fields =["VM_ID", "TIMESTAMP","EVENT_id","EVENT_TYPE","RESOURCES_TYPE","RESOURCES","EVENT_STATUS","NOTBEFORE"]
     rows = [[id, current_time,"NULL","NULL","NULL","NULL","NULL","NULL"]]
     df = pandas.DataFrame(data=rows, columns=fields)
     client.ingest_from_dataframe(df, ingestion_properties=ingestion_props)
   else:
     handle_scheduled_events(data) # function for handling events

main()


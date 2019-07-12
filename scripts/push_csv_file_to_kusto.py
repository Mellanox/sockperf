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
    database="Azurehn", #name of database in kusto
    table="VM_Data",    #name of table in kusto
    dataFormat=DataFormat.csv,
    # incase status update for success are also required
    # reportLevel=ReportLevel.FailuresAndSuccesses,
)
client.ingest_from_file("Delta_Time.csv", ingestion_properties=ingestion_props)


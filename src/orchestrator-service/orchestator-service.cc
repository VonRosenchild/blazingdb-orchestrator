#include <iostream>
#include <map>
#include <tuple>
#include <blazingdb/protocol/api.h>

// TODO: remove global
std::string globalOrchestratorPort;
std::string globalCalciteIphost;
std::string globalCalcitePort;
std::string globalRalIphost;
std::string globalRalPort;

#include "ral-client.h"
#include "calcite-client.h"

#include <blazingdb/protocol/message/interpreter/messages.h>
#include <blazingdb/protocol/message/messages.h>
#include <blazingdb/protocol/message/orchestrator/messages.h>
#include <blazingdb/protocol/message/io/file_system.h>

#include <cstdlib>     /* srand, rand */
#include <ctime>       /* time */

using namespace blazingdb::protocol;
using result_pair = std::pair<Status, std::shared_ptr<flatbuffers::DetachedBuffer>>;

static result_pair registerFileSystem(uint64_t accessToken, Buffer&& buffer)  {
  try {
    interpreter::InterpreterClient ral_client;
    auto response = ral_client.registerFileSystem(accessToken, buffer);

  } catch (std::runtime_error &error) {
    // error with query plan: not resultToken
    std::cout << "In function registerFileSystem: " << error.what() << std::endl;
    std::string stringErrorMessage = "Cannot register the filesystem: " + std::string(error.what());
    ResponseErrorMessage errorMessage{ stringErrorMessage };
    return std::make_pair(Status_Error, errorMessage.getBufferData());
  }
  ZeroMessage response{};
  return std::make_pair(Status_Success, response.getBufferData());
}

static result_pair deregisterFileSystem(uint64_t accessToken, Buffer&& buffer)  {
  try {
    interpreter::InterpreterClient ral_client;
    blazingdb::message::io::FileSystemDeregisterRequestMessage message(buffer.data());
    auto response = ral_client.deregisterFileSystem(accessToken, message.getAuthority());

  } catch (std::runtime_error &error) {
    // error with query plan: not resultToken
    std::cout << "In function deregisterFileSystem: " << error.what() << std::endl;
    std::string stringErrorMessage = "Cannot deregister the filesystem: " + std::string(error.what());
    ResponseErrorMessage errorMessage{ stringErrorMessage };
    return std::make_pair(Status_Error, errorMessage.getBufferData());
  }
  ZeroMessage response{};
  return std::make_pair(Status_Success, response.getBufferData());
}

static result_pair loadCsvSchema(uint64_t accessToken, Buffer&& buffer) {
  std::shared_ptr<flatbuffers::DetachedBuffer> resultBuffer;
   try {
    interpreter::InterpreterClient ral_client;
    resultBuffer = ral_client.loadCsvSchema(buffer, accessToken);

  } catch (std::runtime_error &error) {
    // error with query plan: not resultToken
    std::cout << "In function loadCsvSchema: " << error.what() << std::endl;
    std::string stringErrorMessage = "Cannot load the csv schema: " + std::string(error.what());
    ResponseErrorMessage errorMessage{ stringErrorMessage };
    return std::make_pair(Status_Error, errorMessage.getBufferData());
  }
  return std::make_pair(Status_Success, resultBuffer);
}


static result_pair loadParquetSchema(uint64_t accessToken, Buffer&& buffer) {
  std::shared_ptr<flatbuffers::DetachedBuffer> resultBuffer;
   try {
    interpreter::InterpreterClient ral_client;
    resultBuffer = ral_client.loadParquetSchema(buffer, accessToken);

  } catch (std::runtime_error &error) {
    // error with query plan: not resultToken
    std::cout << "In function loadParquetSchema: " << error.what() << std::endl;
    std::string stringErrorMessage = "Cannot load the parquet schema: " + std::string(error.what());
    ResponseErrorMessage errorMessage{ stringErrorMessage };
    return std::make_pair(Status_Error, errorMessage.getBufferData());
  }
  return std::make_pair(Status_Success, resultBuffer);
}


static result_pair  openConnectionService(uint64_t nonAccessToken, Buffer&& buffer)  {
  srand(time(0));
  int64_t token = rand();
  orchestrator::AuthResponseMessage response{token};
  std::cout << "authorizationService: " << token << std::endl;
  return std::make_pair(Status_Success, response.getBufferData());
};


static result_pair closeConnectionService(uint64_t accessToken, Buffer&& buffer)  {
  try {
    interpreter::InterpreterClient ral_client;
    auto status = ral_client.closeConnection(accessToken);
    std::cout << "status:" << status << std::endl;
  } catch (std::runtime_error &error) {
    std::cout << "In function closeConnectionService: " << error.what() << std::endl;
    std::string stringErrorMessage = "Cannot close the connection: " + std::string(error.what());
    ResponseErrorMessage errorMessage{ stringErrorMessage };
    return std::make_pair(Status_Error, errorMessage.getBufferData());
  }
  ZeroMessage response{};
  return std::make_pair(Status_Success, response.getBufferData());
};

static result_pair dmlFileSystemService (uint64_t accessToken, Buffer&& buffer) {
  blazingdb::message::io::FileSystemDMLRequestMessage requestPayload(buffer.data());
  auto query = requestPayload.statement;
  std::cout << "##DML-FS: " << query << std::endl;
  std::shared_ptr<flatbuffers::DetachedBuffer> resultBuffer;

  try {
    calcite::CalciteClient calcite_client;
    auto response = calcite_client.runQuery(query);
    auto logicalPlan = response.getLogicalPlan();
    auto time = response.getTime();
    std::cout << "plan:" << logicalPlan << std::endl;
    std::cout << "time:" << time << std::endl;
    try {
      interpreter::InterpreterClient ral_client;

      auto executePlanResponseMessage = ral_client.executeFSDirectPlan(logicalPlan, requestPayload.tableGroup, accessToken);

      auto nodeInfo = executePlanResponseMessage.getNodeInfo();
      auto dmlResponseMessage = orchestrator::DMLResponseMessage(
          executePlanResponseMessage.getResultToken(),
          nodeInfo, time);
      resultBuffer = dmlResponseMessage.getBufferData();
    } catch (std::runtime_error &error) {
      // error with query plan: not resultToken
      std::cout << "In function dmlFileSystemService: " << error.what() << std::endl;
      std::string stringErrorMessage = "Error on the communication between Orchestrator and RAL: " + std::string(error.what());
      ResponseErrorMessage errorMessage{ stringErrorMessage };
      return std::make_pair(Status_Error, errorMessage.getBufferData());
    }
  } catch (std::runtime_error &error) {
    // error with query: not logical plan error
    std::cout << "In function dmlFileSystemService: " << error.what() << std::endl;
    std::string stringErrorMessage = "Error on the communication between Orchestrator and Calcite: " + std::string(error.what());
    ResponseErrorMessage errorMessage{ stringErrorMessage };
    return std::make_pair(Status_Error, errorMessage.getBufferData());
  }
  return std::make_pair(Status_Success, resultBuffer);
}

static result_pair dmlService(uint64_t accessToken, Buffer&& buffer)  {
  orchestrator::DMLRequestMessage requestPayload(buffer.data());
  auto query = requestPayload.getQuery();
  std::cout << "DML: " << query << std::endl;
  std::shared_ptr<flatbuffers::DetachedBuffer> resultBuffer;

  try {
    calcite::CalciteClient calcite_client;
    auto response = calcite_client.runQuery(query);
    auto logicalPlan = response.getLogicalPlan();
    auto time = response.getTime();
    std::cout << "plan:" << logicalPlan << std::endl;
    std::cout << "time:" << time << std::endl;
    try {
      interpreter::InterpreterClient ral_client;

      auto executePlanResponseMessage = ral_client.executeDirectPlan(
          logicalPlan, requestPayload.getTableGroup(), accessToken);
      auto nodeInfo = executePlanResponseMessage.getNodeInfo();
      auto dmlResponseMessage = orchestrator::DMLResponseMessage(
          executePlanResponseMessage.getResultToken(),
          nodeInfo, time);
      resultBuffer = dmlResponseMessage.getBufferData();
    } catch (std::runtime_error &error) {
      // error with query plan: not resultToken
      std::cout << "In function dmlService: " << error.what() << std::endl;
      std::string stringErrorMessage = "Error on the communication between Orchestrator and RAL: " + std::string(error.what());
      ResponseErrorMessage errorMessage{ stringErrorMessage };
      return std::make_pair(Status_Error, errorMessage.getBufferData());
    }
  } catch (std::runtime_error &error) {
    // error with query: not logical plan error
    std::cout << "In function dmlService: " << error.what() << std::endl;
    std::string stringErrorMessage = "Error on the communication between Orchestrator and Calcite: " + std::string(error.what());
    ResponseErrorMessage errorMessage{ stringErrorMessage };
    return std::make_pair(Status_Error, errorMessage.getBufferData());
  }
  return std::make_pair(Status_Success, resultBuffer);
};


static result_pair ddlCreateTableService(uint64_t accessToken, Buffer&& buffer)  {
  std::cout << "###DDL Create Table: " << std::endl;
   try {
    calcite::CalciteClient calcite_client;

    orchestrator::DDLCreateTableRequestMessage payload(buffer.data());
    std::cout << "bdname:" << payload.dbName << std::endl;
    std::cout << "table:" << payload.name << std::endl;
    for (auto col : payload.columnNames)
      std::cout << "\ntable.column:" << col << std::endl;

    auto status = calcite_client.createTable(  payload );
  } catch (std::runtime_error &error) {
     // error with ddl query
    std::cout << "In function ddlCreateTableService: " << error.what() << std::endl;
    std::string stringErrorMessage = "In function ddlCreateTableService: cannot create the table: " + std::string(error.what());
    ResponseErrorMessage errorMessage{ stringErrorMessage };
    return std::make_pair(Status_Error, errorMessage.getBufferData());
  }
  ZeroMessage response{};
  return std::make_pair(Status_Success, response.getBufferData());
};


static result_pair ddlDropTableService(uint64_t accessToken, Buffer&& buffer)  {
  std::cout << "##DDL Drop Table: " << std::endl;

  try {
    calcite::CalciteClient calcite_client;

    orchestrator::DDLDropTableRequestMessage payload(buffer.data());
    std::cout << "cbname:" << payload.dbName << std::endl;
    std::cout << "table.name:" << payload.name << std::endl;

    auto status = calcite_client.dropTable(  payload );
  } catch (std::runtime_error &error) {
    // error with ddl query
    std::cout << "In function ddlDropTableService: " << error.what() << std::endl;
    std::string stringErrorMessage = "Orchestrator can't communicate with Calcite: " + std::string(error.what());
    ResponseErrorMessage errorMessage{ stringErrorMessage };
    return std::make_pair(Status_Error, errorMessage.getBufferData());
  }
  ZeroMessage response{};
  return std::make_pair(Status_Success, response.getBufferData());
};


using FunctionType = result_pair (*)(uint64_t, Buffer&&);

static std::map<int8_t, FunctionType> services;
auto orchestratorService(const blazingdb::protocol::Buffer &requestBuffer) -> blazingdb::protocol::Buffer {
  RequestMessage request{requestBuffer.data()};
  std::cout << "header: " << (int)request.messageType() << std::endl;

  auto result = services[request.messageType()] ( request.accessToken(),  request.getPayloadBuffer() );
  ResponseMessage responseObject{result.first, result.second};
  return Buffer{responseObject.getBufferData()};
};

int
main(int argc, const char *argv[]) {
//  if (6 != argc) {
//    std::cout << "usage: " << argv[0]
//              << " <ORCHESTRATOR_PORT> <CALCITE_[IP|HOSTNAME]> <CALCITE_PORT> "
//                 "<RAL_[IP|HOSTNAME]> <RAL_PORT>"
//              << std::endl;
//    return 1;
//  }
//    globalOrchestratorPort = argv[1];
//    globalCalciteIphost    = argv[2];
//    globalCalcitePort      = argv[3];
//    globalRalIphost        = argv[4];
//    globalRalPort          = argv[5];

  std::cout << "Orchestrator is listening" << std::endl;

  blazingdb::protocol::UnixSocketConnection connection("/tmp/orchestrator.socket");
  blazingdb::protocol::Server server(connection);

  services.insert(std::make_pair(orchestrator::MessageType_DML, &dmlService));
  services.insert(std::make_pair(orchestrator::MessageType_DML_FS, &dmlFileSystemService));

  services.insert(std::make_pair(orchestrator::MessageType_DDL_CREATE_TABLE, &ddlCreateTableService));
  services.insert(std::make_pair(orchestrator::MessageType_DDL_DROP_TABLE, &ddlDropTableService));

  services.insert(std::make_pair(orchestrator::MessageType_AuthOpen, &openConnectionService));
  services.insert(std::make_pair(orchestrator::MessageType_AuthClose, &closeConnectionService));

  services.insert(std::make_pair(orchestrator::MessageType_RegisterFileSystem, &registerFileSystem));
  services.insert(std::make_pair(orchestrator::MessageType_DeregisterFileSystem, &deregisterFileSystem));

  services.insert(std::make_pair(orchestrator::MessageType_LoadCsvSchema, &loadCsvSchema));
  services.insert(std::make_pair(orchestrator::MessageType_LoadParquetSchema, &loadParquetSchema));

  server.handle(&orchestratorService);
  return 0;
}

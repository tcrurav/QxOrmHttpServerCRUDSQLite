#include "../include/precompiled.h"
#include "bicycle.h"

#include <QtCore/qcoreapplication.h>

#include <QxOrm_Impl.h>
#include <QtHttpServer>

#define API_KEY "SecretKey"

static bool checkApiKeyHeader(const QList<QPair<QByteArray, QByteArray>> &headers)
{
    for (const auto &[key, value] : headers)
    {
        if (key == "api_key" && value == API_KEY)
        {
            return true;
        }
    }
    return false;
}

static std::optional<QJsonObject> byteArrayToJsonObject(const QByteArray &arr)
{
    QJsonParseError err;
    const auto json = QJsonDocument::fromJson(arr, &err);
    if (err.error || !json.isObject())
        return std::nullopt;
    return json.object();
}

int main(int argc, char *argv[])
{
    // Qt application
    QCoreApplication app(argc, argv);
    QFile::remove("./qxBicycles.sqlite");

    // Parameters to connect to database
    qx::QxSqlDatabase::getSingleton()->setDriverName("QSQLITE");
    qx::QxSqlDatabase::getSingleton()->setDatabaseName("./qxBicycles.sqlite");
    qx::QxSqlDatabase::getSingleton()->setHostName("localhost");
    qx::QxSqlDatabase::getSingleton()->setUserName("root");
    qx::QxSqlDatabase::getSingleton()->setPassword("");
    qx::QxSqlDatabase::getSingleton()->setFormatSqlQueryBeforeLogging(true);
    qx::QxSqlDatabase::getSingleton()->setDisplayTimerDetails(true);

    // Only for debug purpose : assert if invalid offset detected fetching a relation
    qx::QxSqlDatabase::getSingleton()->setVerifyOffsetRelation(true);

    // Create all tables in database
    QSqlError daoError = qx::dao::create_table<bicycle>();
    daoError = qx::dao::create_table<bicycle>();

    // Create a list of 3 bicycles
    bicycle_ptr bicycle_1;
    bicycle_1.reset(new bicycle());
    bicycle_ptr bicycle_2;
    bicycle_2.reset(new bicycle());
    bicycle_ptr bicycle_3;
    bicycle_3.reset(new bicycle());

    bicycle_1->m_id = 1;
    bicycle_1->m_brand = "BH";
    bicycle_1->m_model = "Star";
    bicycle_2->m_id = 2;
    bicycle_2->m_brand = "Orbea";
    bicycle_2->m_model = "Sky";
    bicycle_3->m_id = 3;
    bicycle_3->m_brand = "Decathlon";
    bicycle_3->m_model = "Super";

    list_bicycle bicycleX;
    bicycleX.push_back(bicycle_1);
    bicycleX.push_back(bicycle_2);
    bicycleX.push_back(bicycle_3);

    // Insert list of 3 bicycles into database
    daoError = qx::dao::insert(bicycleX);

    QHttpServer httpServer;

    httpServer.route(
        "/", []()
        { return "Bicycles API using QxOrm & QtHttpServer"; });

    httpServer.route(
        "/v2/bicycle", QHttpServerRequest::Method::Get,
        [](const QHttpServerRequest &)
        {
            QSqlError daoError;
            QStringList lstColumns = QStringList() << "bicycle_id"
                                                   << "bicycle_brand"
                                                   << "bicycle_model";
            list_bicycle lst_bicycle;
            daoError = qx::dao::fetch_all(lst_bicycle, NULL, lstColumns);

            QJsonArray array;
            std::transform(lst_bicycle.cbegin(), lst_bicycle.cend(),
                           std::inserter(array, array.begin()),
                           [](const auto &it)
                           {
                               return QJsonObject{{"id", (int const)(it->m_id)},
                                                  {"brand", QJsonValue(it->m_brand)},
                                                  {"model", QJsonValue(it->m_model)}};
                           });

            return QHttpServerResponse(array);
        });

    httpServer.route(
        "/v2/bicycle/<arg>", QHttpServerRequest::Method::Get,
        [](qint64 bicycleId, const QHttpServerRequest &)
        {
            QSqlError daoError;
            qx::dao::ptr<bicycle> bicycle_aux = qx::dao::ptr<bicycle>(new class bicycle());
            bicycle_aux->m_id = bicycleId;
            daoError = qx::dao::fetch_by_id(bicycle_aux);

            return !daoError.isValid()
                       ? QHttpServerResponse(QJsonObject{{"id", (int const)(bicycle_aux->m_id)},
                                                         {"brand", QJsonValue(bicycle_aux->m_brand)},
                                                         {"model", QJsonValue(bicycle_aux->m_model)}})
                       : QHttpServerResponse(QHttpServerResponder::StatusCode::NotFound);
        });

    httpServer.route(
        "/v2/bicycle", QHttpServerRequest::Method::Post,
        [](const QHttpServerRequest &request)
        {
            if (!checkApiKeyHeader(request.headers()))
            {
                return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);
            }
            const auto json = byteArrayToJsonObject(request.body());
            if (!json || !json->contains("brand") || !json->contains("model"))
                return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);

            bicycle_ptr bicycle_aux;
            bicycle_aux.reset(new bicycle());

            bicycle_aux->m_brand = json->value("brand").toString();
            bicycle_aux->m_model = json->value("model").toString();

            QSqlError daoError;
            daoError = qx::dao::insert(bicycle_aux);

            return !daoError.isValid()
                       ? QHttpServerResponse(QHttpServerResponder::StatusCode::Created)
                       : QHttpServerResponse(QHttpServerResponder::StatusCode::InternalServerError);
        });

    httpServer.route(
        "/v2/bicycle/<arg>", QHttpServerRequest::Method::Put,
        [](qint64 bicycleId, const QHttpServerRequest &request)
        {
            if (!checkApiKeyHeader(request.headers()))
            {
                return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);
            }
            const auto json = byteArrayToJsonObject(request.body());
            if (!json || !json->contains("brand") || !json->contains("model"))
            {
                return QHttpServerResponse(QHttpServerResponder::StatusCode::BadRequest);
            }

            QSqlError daoError;
            qx::dao::ptr<bicycle> bicycle_aux = qx::dao::ptr<bicycle>(new class bicycle());
            bicycle_aux->m_id = bicycleId;
            daoError = qx::dao::fetch_by_id(bicycle_aux);

            if (daoError.isValid())
                return QHttpServerResponse(QHttpServerResponder::StatusCode::NotFound);

            bicycle_aux->m_brand = json->value("brand").toString();
            bicycle_aux->m_model = json->value("model").toString();

            daoError = qx::dao::save(bicycle_aux);

            return !daoError.isValid()
                       ? QHttpServerResponse(QHttpServerResponder::StatusCode::Ok)
                       : QHttpServerResponse(QHttpServerResponder::StatusCode::InternalServerError);
        });

    httpServer.route(
        "/v2/bicycle/<arg>", QHttpServerRequest::Method::Delete,
        [](qint64 bicycleId, const QHttpServerRequest &request)
        {
            if (!checkApiKeyHeader(request.headers()))
            {
                return QHttpServerResponse(QHttpServerResponder::StatusCode::Unauthorized);
            }
            QSqlError daoError;
            qx::dao::ptr<bicycle> bicycle_aux = qx::dao::ptr<bicycle>(new class bicycle());
            bicycle_aux->m_id = bicycleId;
            daoError = qx::dao::delete_by_id(bicycle_aux);
            if (daoError.isValid())
                return QHttpServerResponse(QHttpServerResponder::StatusCode::NoContent);
            return QHttpServerResponse(QHttpServerResponder::StatusCode::Ok);
        });

    const auto port = httpServer.listen(QHostAddress::Any, 49080);
    if (!port)
    {
        qDebug() << QCoreApplication::translate("QHttpServerExample",
                                                "Server failed to listen on a port.");
        return 0;
    }

    qDebug() << QCoreApplication::translate(
                    "QHttpServerExample",
                    "Running on http://127.0.0.1:%1/ (Press CTRL+C to quit)")
                    .arg(port);

    return app.exec();
}

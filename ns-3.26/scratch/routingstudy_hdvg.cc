#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mobility-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
//#include "ns3/dvg-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/yans-wifi-helper.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <math.h>
#include<stdio.h>
#include <sstream>

//#include "ns3/visualizer.h"
using namespace std;

NS_LOG_COMPONENT_DEFINE ("ROUTING_STUDY");

using namespace ns3;
class TrafStat
{
public:
  TrafStat()
  {
    rx_packets = 0;
    tx_packets = 0;
    lost_packets = 0;
    mean_delay = 0;
    mean_jitter = 0;
    avrg_hops = 0;
    route_establishing_time = 0;
    pdr = 0;
    conns = 0;
    conns_succeed = 0;
  }

  void CalculateFinalStats()
  {
    mean_delay = mean_delay/conns_succeed/10e9;
    mean_jitter= mean_jitter/conns_succeed/10e9;
    avrg_hops = avrg_hops/conns_succeed;
    route_establishing_time = route_establishing_time/(double)conns_succeed/10e9;
    if (tx_packets != 0)
      pdr = (double)rx_packets/(double)tx_packets*100;
  }

  //Число принятых за симуляцию пакетов
  uint32_t rx_packets;
  //Число отправленных за симуляцию пакетов
  uint32_t tx_packets;
  //Число утерянных за симуляцию пакетов
  uint32_t lost_packets;
  //Средняя задержка доставки пакета
  double mean_delay;
  //Cреднее значение джиттера
  double mean_jitter;
  //Среднее число промежуточных узлов, пройденных пакетами
  double avrg_hops;
  //Среднее время установления соединения
  double route_establishing_time;
  //Коэффициент доставки пакетов
  double pdr;
  //Число потоков
  uint32_t conns;
  //Число потоков, в которых был прият хотя бы один пакет
  uint32_t conns_succeed;

};
class Experiment
{
public:
  Experiment()
  {
    /*************** Общие параметры симуляции *****************/
    protocol                  = 6;
    nWifis                    = 20;
    nSinks                    = 2;
    nSinksVar                 = false;
    TotalTime                 = 150;
    /*************** Параметры трафика *****************/
    warmUpTime                = 95;
    coolDownTime              = 5;
    dataPort                  = 9;
    control_port              = 776;
    dataRate                  = "5300bps";
    dataPacketSize            = 20;
    /*************** Параметры мобильности ****************/
    distance                  = 150;
    minNodeSpeed              = 0.00001;
    maxNodeSpeed              = 0.00001;
    nodePause                 = 0;
    nodePosMode               = 2;
    /*************** Параметры радиоканала ****************/
    txp                       = 7.5;
    macMode                   = "DsssRate11Mbps";
    /*************** Префиксы выходных файлов ****************/
    protocolName              = "protocol";
    prefix                    = "routingstudy";
    /*************** Параметры логирования ****************/
    traceMobility             = false;
    enablePcap                = false;
    showTime                  = true;
    /*************** Параметры DVG протокола *****************/
    MaxClusterSize            = 30;
    ObservationTimeL2         = 45;
    helloPeriodicityL1        = 2;
    ObservationTimeL1         = helloPeriodicityL1;
    SendQueuePeriod           = 0.1;
    SendQueuePeriodL2         = 0.1;
    SRRType                   = 0;
    SRRC                      = 0;
    MaxHelloQueueLength       = nWifis+1;
    MaxRgeoQueueLength        = nWifis+1;
    helloPeriodicityl2        = helloPeriodicityL1;
    maxMovement               = 20;
    noRebroadcastRange        = 0;
    selfIDEntryLifeTime       = 2*helloPeriodicityL1;
    RoutingTableEntryLifeTime = 2*helloPeriodicityL1;

    /*************** Метрики *****************/
    logPHYRxDrops = false;
    logPHYTxDrops = false;
    phyRxPacketDroped = 0;
    phyRxBytesDroped = 0;
    phyRxPacketBegin = 0;
    phyRxBytesBegin = 0;

    m_IDREQsent = 0;
    m_IDREQrecv = 0;
    m_IDREPsent = 0;
    m_IDREPrecv = 0;
  }

  ~Experiment()
  {
  }


  /*************** Запрос переменных из коммандной строки ***************/
  void
  GetArgs(int argc, char *argv[])
  {
    CommandLine cmd;
    cmd.AddValue ("prefix", "Префикс имени выходных файлов "
        "[string, по умолчанию 'routingstudy']", prefix);
    cmd.AddValue ("traceMobility", "Запись перемещения узлов в файл "
        "[bool, По умолчанию 0]", traceMobility);
    cmd.AddValue ("enablePcap", "Запись трафика в pcap файлы  "
        "[bool, по умолчанию 0]", enablePcap);
    cmd.AddValue ("showTime", "Вывод в консоль текущего времени симуляции "
        "[bool, по умолчанию 1]", showTime);
    cmd.AddValue ("nWifis", "Число узлов в сети "
        "[по умолчанию 100]", nWifis);
    cmd.AddValue ("nSinks", "Число потоков с данными "
        "[по умолчанию 1]", nSinks);
    cmd.AddValue ("nSinksVar", "Использовать число потоков с данными "
        "пропорциональное размеру сети (0.1*Wifis) [по умолчанию 0]", nSinksVar);
    cmd.AddValue ("TotalTime", "Общее время симуляции, с. "
        "[по умолчанию 150]", TotalTime);
    cmd.AddValue ("distance", "Средняя дистанция между узлами по осям X и Y,"
        " м. [по умолчанию 150]", distance);
    cmd.AddValue ("dataPacketSize", "Размер пакета данных (без IP и"
        " UDP заголовков), байт [по умолчанию 20, G.723.1-5.3]",
        dataPacketSize);
    cmd.AddValue ("dataRate", "Скорость потока данных, бит/с "
        "[по умолчанию 5300, G.723.1-5.3]", dataRate);
    cmd.AddValue ("macMode", "протокол MAC уровня"
        "[по умолчанию DsssRate11Mbps]", macMode);
    cmd.AddValue ("warmUpTime", "Время начала генерации трафика "
        "[по умолчанию 95 с]", warmUpTime);
    cmd.AddValue ("coolDownTime", "Время после окончания генерации трафика "
        "до конца симуляции [по умолчанию 5 с]", coolDownTime);
    cmd.AddValue ("minNodeSpeed", "Минимальная скорость движения узлов"
        " [по умолчанию 0.00001 м]", minNodeSpeed);
    cmd.AddValue ("maxNodeSpeed", "Максимальная скорость движения узлов"
        " [по умолчанию 0.00001 м]", maxNodeSpeed);
    cmd.AddValue ("nodePause", "Пауза между интервалами движения узла"
        " [по умолчанию 0 c]", nodePause);
    cmd.AddValue ("txp", "Коэффициент усиления передающей антенны"
        " [по умолчанию 7.5 дБм]", txp);
    cmd.AddValue ("protocol", "1=DVH,2=DVG,3=HDVH,4=HDVG,5=OLSR;"
        "6=AODV;7=DSDV,8=GPSR, [по умолчанию 4]", protocol);
    cmd.AddValue ("nodePosMode", "Расстановка узлов, 1 - случайным образом,"
        " 2 - равномерная на сетке, 3 - в линию"
        " [по умолчанию 1]", nodePosMode);
    cmd.AddValue ("helloPeriodicityL1", "Периодичность рассылки"
        " HELLO-пакетов [по умолчанию 5 с]", helloPeriodicityL1);
    cmd.AddValue ("SendQueuePeriod", "Максимальное значение задержки перед"
        " следующим опустошением буфера [по умолчанию 0.1 с]",
        SendQueuePeriod);
    cmd.AddValue ("SendQueuePeriodL2", "Максимальное значение задержки перед"
        " следующим опустошением буфера [по умолчанию 0.1 с]",
        SendQueuePeriodL2);
    cmd.AddValue ("maxMovement", "Норма вектора перемещения узла,"
        " при превышении которой он рассылает обновленные координаты"
        " [по умолчанию 20 м]. Только для DVG и HDVG", maxMovement);
    cmd.AddValue ("noRebroadcastRange", "Радиус зоны вокруг узла, внутри"
        " которой запрещена ретрансляция широковещательных пакетов"
        " от этого узла [по умолчанию 0 - ограничение отсутствует]."
        " Только для DVG и HDVG.", noRebroadcastRange);
    cmd.AddValue ("SRRType", "Тип стохастического ограниения: "
        "1 - ограничение при вставке в очередь, "
        "2 - ограничение при опустошении очереди", SRRType);
    cmd.AddValue ("SRRC", "Коэффициент N для стохастического"
        " ограничения ретрансляции. Вероятность ретрансляции равна"
        " N/{число соседей узла}. "
        "[по умолчанию 0 - ограничение отсутствует]", SRRC);
    cmd.AddValue ("MaxClusterSize", "Максимальный размер кластера "
        "[по умолчанию 30]", MaxClusterSize);
    cmd.AddValue ("ObservationTimeL2", "Время начала рассылки HELLO 2 уровня "
        "[по умолчанию 45]", ObservationTimeL2);
    cmd.AddValue ("ObservationTimeL1", "Время начала рассылки HELLO 1 уровня "
        "[по умолчанию равно 1 HELLO-интервалу L1]", ObservationTimeL1);
    cmd.AddValue ("MaxHelloQueueLength", "Размер очереди сообщений HELLO первого уровня "
        "[по умолчанию равно nWifis+1]", MaxHelloQueueLength);
    cmd.AddValue ("MaxRgeoQueueLength", "Размер очереди сообщений RGEO первого уровня "
        "[по умолчанию равно nWifis+1]", MaxRgeoQueueLength );
    cmd.AddValue ("helloPeriodicityl2", "Период рассылки HELLO L2 "
        "[по умолчанию равно helloPeriodicity]", helloPeriodicityl2);
    cmd.AddValue ("selfIDEntryLifeTime", "Время в протоколе HDVG(H), в течение которого"
        "узел после вступления в кластер выключается из процесса кластеризации",
        selfIDEntryLifeTime);
    cmd.AddValue ("RoutingTableEntryLifeTime", "Время жизни записи в таблице маршрутизации",
        RoutingTableEntryLifeTime);
    cmd.AddValue ("logPHYRxDrops", "Записывать число отказов в приеме пакетов на физическом уровне [0]",
        logPHYRxDrops);
    cmd.AddValue ("logPHYTxDrops", "Записывать число отказов в отправке  пакетов на физическом уровне [0]",
        logPHYTxDrops);
    int a;
    cmd.AddValue ("getArgs", "Сохраняет список аргументов в файл keylist",
        a);

    cmd.Parse (argc, argv);

    //Число одновременно активных потоков передачи
    if (nSinksVar == true)
      nSinks = nWifis>=20?nWifis/10:1;

    if (TotalTime<=warmUpTime+coolDownTime)
      {
        NS_FATAL_ERROR("Общее время симуляции должно быть больше "
            << warmUpTime+coolDownTime << " с.");
      }
    if (nWifis>16384)
      {
        NS_FATAL_ERROR("Адресация позволяет симулировать не более 16384 узлов");
      }

    if (nWifis < MaxClusterSize) MaxClusterSize = nWifis;

    phyRxDropedStat = new double[int(TotalTime)];
  }

  void PrintArgs(std::ofstream& os)
  {    
    os << "#*************** Общие параметры симуляции *************#" << endl;
    os << "--protocol=" << protocol << endl;
    os << "--nWifis=" << nWifis << endl;
    os << "--nSinks=" << nSinks << endl;
    os << "#Использовать число потоков с данными пропорциональное размеру сети (0.1*Wifis)" << endl;
    os << "--nSinksVar=" << nSinksVar << endl;
    os << "--TotalTime=" << TotalTime << endl;
    os << "#*************** Параметры трафика *********************#" << endl;
    os << "--warmUpTime=" << warmUpTime << endl;
    os << "--coolDownTime=" << coolDownTime << endl;
    os << "--dataRate=" << dataRate << endl;
    os << "--dataPacketSize=" << dataPacketSize << endl;
    os << "#*************** Параметры мобильности ****************#" << endl;
    os << "--distance=" << distance << endl;
    os << "--minNodeSpeed=" << minNodeSpeed << endl;
    os << "--maxNodeSpeed=" << maxNodeSpeed << endl;
    os << "--nodePause=" << nodePause << endl;
    os << "--nodePosMode=" << nodePosMode << endl;
    os << "#*************** Параметры радиоканала ****************#" << endl;
    os << "--txp=" << txp << endl;
    os << "--macMode=" << macMode << endl;
    os << "#*************** Префиксы выходных файлов *************#" << endl;
    os << "--prefix=" << prefix << endl;
    os << "#*************** Параметры логирования ****************#" << endl;
    os << "--traceMobility=" << traceMobility << endl;
    os << "--enablePcap=" << enablePcap << endl;
    os << "--showTime=" << showTime << endl;
    os << "--logPHYRxDrops=" << logPHYRxDrops << endl;
    os << "--logPHYTxDrops=" << logPHYTxDrops << endl;
    os << "#*************** Параметры DVG протокола **************#" << endl;
    os << "--MaxClusterSize=" << MaxClusterSize << endl;
    os << "--helloPeriodicityL1=" << helloPeriodicityL1 << endl;
    os << "--helloPeriodicityl2=" << helloPeriodicityl2 << endl;
    os << "--ObservationTimeL1=" << ObservationTimeL1 << endl;
    os << "--ObservationTimeL2=" << ObservationTimeL2 << endl;
    os << "--SendQueuePeriod=" << SendQueuePeriod << endl;
    os << "--SendQueuePeriodL2=" << SendQueuePeriodL2 << endl;
    os << "--MaxHelloQueueLength=" << MaxHelloQueueLength << endl;
    os << "--MaxRgeoQueueLength=" << MaxRgeoQueueLength << endl;
    os << "--SRRType=" << SRRType << endl;
    os << "--SRRC=" << SRRC << endl;
    os << "--maxMovement=" << maxMovement << endl;
    os << "--noRebroadcastRange=" << noRebroadcastRange << endl;
    os << "--selfIDEntryLifeTime=" << selfIDEntryLifeTime << endl;
    os << "--RoutingTableEntryLifeTime=" << RoutingTableEntryLifeTime << endl;
  }

  void PrintResults (std::ofstream& os)
  {
    double controlTx = ctlStatL1.tx_packets+ctlStatL2.tx_packets;
    double controlRx = ctlStatL1.rx_packets+ctlStatL2.rx_packets;
    double controlLost = ctlStatL1.lost_packets+ctlStatL2.lost_packets;
    double controlPdr = controlRx/controlTx*100;
    double dataTx = dataStat.tx_packets;
    double dataRx = dataStat.rx_packets;
    double dataLost = dataStat.lost_packets;
    double dataPdr = dataRx/dataTx*100;
    overhead = controlTx/(dataTx+controlTx)*100;

    os << "Параметры симуляции:                                  " << endl;
    os << "                                                      " << endl;
    os << "======================================================" << endl;
    os << "Протокол маршрутизации:                               " << protocolName << endl;
    os << "Число узлов в сети:                                   " << nWifis << endl;
    os << "Средняя дистанция между узлами по осям X и Y, м:      " << distance << endl;
    os << "Скорость потока данных, бит/с:                        " << dataRate << endl;
    os << "Размер пакета данных (без IP и UDP заголовков), байт: " << dataPacketSize << endl;
    os << "Скорость передвижения узлов (мин-макс) ,м/с:          " << minNodeSpeed <<
        " - " << maxNodeSpeed << endl;
    os << "Общее время симуляции, с:                             " << TotalTime << endl;
    os << "Интервал активности генераторов трафика, с:           " << warmUpTime <<
        " - " << TotalTime-coolDownTime << endl;
    os << "======================================================" << endl;
    os << "                                                      " << endl;
    os << "                                                      " << endl;
    os << "Результаты симуляции:                                 " << endl;
    os << "                                                      " << endl;
    os << "=======================================================================" << endl;
    os << "Коэффициент связности:                                                 " << linking << endl;
    os << "-----------------------------------------------------------------------" << endl;
    os << "Отправлено пакетов с данными:                                          " << dataTx  << endl;
    os << "Отправлено служебных пакетов:                                          " << controlTx << endl;
    os << "Доля служебного трафика:                                               " << overhead << "%" << endl;
    os << "-----------------------------------------------------------------------" << endl;
    os << "Отправлено пакетов с данными:                                          " << dataTx << endl;
    os << "Принято пакетов с данными:                                             " << dataRx << endl;
    os << "Потеряно пакетов с данными:                                            " << dataLost << endl;
    os << "Коэффициент доставки пакетов с данными:                               " << dataPdr << "%" << endl;
    os << "НормКоэфф   доставки пакетов с данными:                               " << dataPdr*dataStat.conns_succeed/dataStat.conns << "%" << endl;
    os << "-----------------------------------------------------------------------" << endl;
    os << "Отправлено пакетов со служебной информации:                            " << controlTx << endl;
    os << "Принято пакетов со служебной информации:                               " << controlRx << endl;
    os << "Потеряно пакетов со служебной информации:                              " << controlLost << endl;
    os << "Коэффициент доставки пакетов со служебной информации:                  " << controlPdr << "%" << endl;
    os << "-----------------------------------------------------------------------" << endl;
    os << "Среднее время открытия маршрута " << endl;
    os << "(включая время прохождения первого пакета по линии связи):              ";
    if (dataStat.conns_succeed==0)
      os << "не было открыто ни одного маршрута" << endl;
    else
      os << dataStat.route_establishing_time << " (маршрут открыт в " <<
      dataStat.conns_succeed << " из " << dataStat.conns << " потоков)" << endl;    
    os << "Среднее время доставки пакета:                                          ";
    if (dataStat.conns_succeed==0)
      os << "не было открыто ни одного маршрута" << endl;
    else
      os << dataStat.mean_delay << " (маршрут открыт в " <<
      dataStat.conns_succeed << " из " << dataStat.conns << " потоков)" << endl;
    os << "Средний джиттер:                                                        ";
    if (dataStat.conns_succeed==0)
      os << "не было открыто ни одного маршрута" << endl;
    else
      os << dataStat.mean_jitter << " (маршрут открыт в " <<
      dataStat.conns_succeed << " из " << dataStat.conns << " потоков)" << endl;
  }

  void PrintMetrics(std::ofstream& os)
  {
    os << "Отправлено пакетов со служебной информации L1:             " << ctlStatL1.tx_packets << endl;
    os << "Принято пакетов со служебной информации L1:                " << ctlStatL1.rx_packets << endl;
    os << "Потеряно пакетов со служебной информации L1:               " << ctlStatL1.lost_packets << endl;
    os << "Коэффициент доставки пакетов со служебной информации L1:   " << ctlStatL1.pdr << "%" << endl;
    os << "Всего сессий передачи L1:                                  " << ctlStatL1.conns << endl;
    os << "Успешных сессий передачи L1:                               " << ctlStatL1.conns_succeed << endl;
    os << "Общее число промежуточных хопов L1:                        " << ctlStatL1.avrg_hops << endl;
    os << "-----------------------------------------------------------------------" << endl;
    os << "Отправлено пакетов со служебной информации L2:             " << ctlStatL2.tx_packets << endl;
    os << "Принято пакетов со служебной информации L2:                " << ctlStatL2.rx_packets << endl;
    os << "Потеряно пакетов со служебной информации L2:               " << ctlStatL2.lost_packets << endl;
    os << "Коэффициент доставки пакетов со служебной информации L2:   " << ctlStatL2.pdr << "%" << endl;
    os << "Всего сессий передачи HELLO L2:                            " << ctlStatL2.conns << endl;
    os << "Успешных сессий передачи HELLO L2:                         " << ctlStatL2.conns_succeed << endl;
    os << "Общее число промежуточных хопов HELLO L2:                  " << ctlStatL2.avrg_hops << endl;
    if(logPHYRxDrops)
      {
        os << "-----------------------------------------------------------------------" << endl;
        os << "Число RxInUse, :                                           "  << phyRxPacketDroped << endl;
        os << "Байт  RxInUse, :                                           "  << phyRxBytesDroped << endl;
        os << "Число RxBegin, :                                           "  << phyRxPacketBegin << endl;
        os << "Байт  RxBegin, :                                           "  << phyRxBytesBegin << endl;
        os << "RxInUse пакетов, %, :                                      "  << (double)phyRxPacketDroped/phyRxPacketBegin*100 << endl;
        for(int i = 0; i< TotalTime; i++)
          {
            os << "RxInUse пакетов [" << i << " c.]: "  << phyRxDropedStat[i]  << endl;
          }
      }
    if(protocolName== "DVG" || protocolName == "DVH" ||
       protocolName == "HDVG" || protocolName == "HDVH")
      {
        os << "-----------------------------------------------------------------------" << endl;
        os << "Отправлено IDREQ:                                          " << m_IDREQsent << endl;
        os << "Принято IDREQ:                                             " << m_IDREQrecv << endl;
        os << "Отправлено IDREP:                                          " << m_IDREPsent << endl;
        os << "Принято IDREP:                                             " << m_IDREPrecv << endl;
      }
    delete [] phyRxDropedStat;
  }

  int
  Init(int argc, char *argv[])
  {
    int i = argc;
    while(i > 1)
      {
        std::string param = argv[i-1];
        if(param == "--getArgs")
          {
            std::ofstream os ("keylist", std::ios::out);
            PrintArgs(os);
            os.close();
            return -1;
          }
        i--;
      }

    GetArgs(argc, argv);
    InitNetwork();

    InitRouting();

    InitInternetStack();

    initTraffic();
    InitMobility();
    InitSniffer();
    return 0;
  }

  void
  InitNetwork() 
  {
    //Если выбрана опция отображения времени симуляции, вызов функции ShowTime()
    //Вызов осуществляется в самом начале инициализации симуляции, для того,
    //чтобы планировщик отсчета временени был инициализирован до всех остальных
    //таймеров
    if (showTime == true) ShowTime();

    //Инициализация сети
    //Создание узлов числом nWifis
    adhocNodes.Create (nWifis);
    //Создание хелпера для инсталляции на узлы WiFi
    WifiHelper wifi;
    //Установка режима работы WiFi согласно спецификации IEEE 802.11g
    wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
    //Постоянная битовая скорость для служебного трафика
    //и данных, метод расщирения спектра DSS
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
        "DataMode",StringValue  (macMode),
        "ControlMode",StringValue (macMode));
    //Установка режима работы maс уровня wifi для неодноадресных передач
    //согласно переменной macMode
    Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
        StringValue (macMode));
    //Создание хелпера для описания физического уровня WiFi
    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
    //Создание хелпера для описания WiFi канала
    YansWifiChannelHelper wifiChannel;
	
    //Установка задержки передачи в WiFi канале в соответствии
    //с постоянной скоростью распространения сигнала в среде
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    //Установка потерь передачи в WiFi канале
    wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    //Создание WiFi канала и инсталляция его на физический уровень
    wifiPhy.SetChannel (wifiChannel.Create ());
    //Установка мощности передатчиков, дБм
    wifiPhy.Set ("TxPowerStart",DoubleValue (txp));
    wifiPhy.Set ("TxPowerEnd", DoubleValue (txp));
    //Коэффициенты усиления передающей и приемной антенны, дБм
    wifiPhy.Set ("TxGain",DoubleValue (0.0));
    wifiPhy.Set ("RxGain",DoubleValue (0.0));
    //Создание хелпера для инсталляции MAC уровня
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
    //Тип - AdhocWifiMac
    wifiMac.SetType ("ns3::AdhocWifiMac");
    //Установка устройств WiFi на узлы в сети
    adhocDevices = wifi.Install (wifiPhy, wifiMac, adhocNodes);

    /*************** Запись трафика в файл *****************/
    //Если выбрана опция записи трафика в файл
    if (enablePcap==true) {
        //записывать трафик в файл 'experiment_name'.pcap
        wifiPhy.EnablePcapAll(experiment_name);
    }
  }

  void
  InitRouting()
  {
    //Создание хелперов для инсталляции протоколов маршрутизации на узлы сети
    AodvHelper aodv;
    aodv.Set("HelloInterval", TimeValue(Seconds(helloPeriodicityL1)));
        aodv.Set("RreqRetries", UintegerValue (2));
        aodv.Set("RreqRateLimit", UintegerValue (10));
        aodv.Set("RerrRateLimit", UintegerValue (10));
        aodv.Set("NodeTraversalTime", TimeValue (MilliSeconds (40)));
        aodv.Set("ActiveRouteTimeout", TimeValue (Seconds (3)));
        aodv.Set("MyRouteTimeout", TimeValue (Seconds (11.2)));
        aodv.Set("BlackListTimeout", TimeValue (Seconds (5.6)));
        aodv.Set("DeletePeriod", TimeValue (Seconds (15)));
        aodv.Set("TimeoutBuffer", UintegerValue (2));
        aodv.Set("NetDiameter", UintegerValue (35));
        aodv.Set("NetTraversalTime", TimeValue (Seconds (2.8)));
        aodv.Set("PathDiscoveryTime", TimeValue (Seconds (5.6)));
        aodv.Set("MaxQueueLen", UintegerValue (64));
        aodv.Set("MaxQueueTime", TimeValue (Seconds (30)));
        aodv.Set("AllowedHelloLoss",  UintegerValue (2));
        aodv.Set("GratuitousReply", BooleanValue (true));
        aodv.Set("DestinationOnly", BooleanValue (false));
        aodv.Set("EnableHello", BooleanValue (true));
        aodv.Set("EnableBroadcast", BooleanValue (true));
    OlsrHelper olsr;
    olsr.Set("HelloInterval", TimeValue(Seconds(helloPeriodicityL1)));
    olsr.Set("TcInterval", TimeValue(Seconds(helloPeriodicityL1/2)));
    olsr.Set("MidInterval", TimeValue(Seconds(helloPeriodicityL1/2)));
    olsr.Set("HnaInterval", TimeValue(Seconds(helloPeriodicityL1/2)));
    DsdvHelper dsdv;
    //    dsdv.Set("PeriodicUpdateInterval", TimeValue(Seconds(15)));
    //    dsdv.Set("SettlingTime", TimeValue(Seconds(5)));
    //    dsdv.Set("MaxQueueLen", UintegerValue(500));
    //    dsdv.Set("MaxQueuedPacketsPerDst", UintegerValue (5));
    //    dsdv.Set("MaxQueueTime", TimeValue(Seconds(30)));
    //    dsdv.Set("EnableBuffering", BooleanValue(true));
    //    dsdv.Set("EnableWST", BooleanValue(true));
    //    dsdv.Set("Holdtimes", UintegerValue(3));
    //    dsdv.Set("WeightedFactor", DoubleValue (0.875));
    //    dsdv.Set("EnableRouteAggregation", BooleanValue(false));
    //    dsdv.Set("RouteAggregationTime", TimeValue (Seconds (1)));
    //*GpsrHelper gpsr;
    //gpsr.Set("HelloInterval", TimeValue(Seconds(helloPeriodicityL1)));

    /*DVGHelper dvh = DVGHelper();
    dvh.Set("UseGeoData",BooleanValue(false));
    dvh.Set("UseHierarchy",BooleanValue(false));
    dvh.Set("HelloIntervalL1",TimeValue(Seconds(helloPeriodicityL1)));
    dvh.Set("ObservationTimeL1",IntegerValue(ObservationTimeL1));
    dvh.Set("SendQueuePeriod",TimeValue(Seconds(SendQueuePeriod)));
    dvh.Set("StohasticRebroadcastRestrictionType",IntegerValue(SRRType));
    dvh.Set("StohasticRebroadcastRestrictionCoefficient",DoubleValue(SRRC));
    dvh.Set("MaxHelloQueueLength",IntegerValue(MaxHelloQueueLength));
    dvh.Set("MaxRgeoQueueLength",IntegerValue(MaxRgeoQueueLength));
    dvh.Set("RoutingTableEntryLifeTime", TimeValue(Seconds(RoutingTableEntryLifeTime)));*/


   /* DVGHelper dvg = DVGHelper();
    dvg.Set("UseGeoData",BooleanValue(true));
    dvg.Set("UseHierarchy",BooleanValue(false));
    dvg.Set("HelloIntervalL1", TimeValue(Seconds(helloPeriodicityL1)));
    dvg.Set("ObservationTimeL1",IntegerValue(ObservationTimeL1));
    dvg.Set("SendQueuePeriod",TimeValue(Seconds(SendQueuePeriod)));
    dvg.Set("StohasticRebroadcastRestrictionType",IntegerValue(SRRType));
    dvg.Set("StohasticRebroadcastRestrictionCoefficient",DoubleValue(SRRC));
    dvg.Set("MaxHelloQueueLength",IntegerValue(MaxHelloQueueLength));
    dvg.Set("MaxRgeoQueueLength",IntegerValue(MaxRgeoQueueLength));
    dvg.Set("RoutingTableEntryLifeTime", TimeValue(Seconds(RoutingTableEntryLifeTime)));*/

   /* dvg.Set("MovementTreshold",DoubleValue(maxMovement));
    dvg.Set("NoRebroadcastRange",DoubleValue(noRebroadcastRange));

    DVGHelper hdvh = DVGHelper();
    hdvh.Set("UseGeoData",BooleanValue(false));
    hdvh.Set("UseHierarchy",BooleanValue(true));
    hdvh.Set("HelloIntervalL1", TimeValue(Seconds(helloPeriodicityL1)));
    hdvh.Set("ObservationTimeL1",IntegerValue(ObservationTimeL1));
    hdvh.Set("SendQueuePeriod",TimeValue(Seconds(SendQueuePeriod)));
    hdvh.Set("SendQueuePeriodL2",TimeValue(Seconds(SendQueuePeriodL2)));
    hdvh.Set("StohasticRebroadcastRestrictionType",IntegerValue(SRRType));
    hdvh.Set("StohasticRebroadcastRestrictionCoefficient",DoubleValue(SRRC));
    hdvh.Set("MaxHelloQueueLength",IntegerValue(MaxHelloQueueLength));
    hdvh.Set("MaxRgeoQueueLength",IntegerValue(MaxRgeoQueueLength));
    hdvh.Set("RoutingTableEntryLifeTime", TimeValue(Seconds(RoutingTableEntryLifeTime)));*/

   /* hdvh.Set("HelloIntervalL2", TimeValue(Seconds(helloPeriodicityl2)));
    hdvh.Set("ObservationTimeL2",IntegerValue(ObservationTimeL2));
    hdvh.Set("MaxClusterSize",IntegerValue(MaxClusterSize));
    hdvh.Set("SelfIDEntryLifeTime", TimeValue(Seconds(selfIDEntryLifeTime)));

    DVGHelper hdvg = DVGHelper();
    hdvg.Set("UseGeoData",BooleanValue(true));
    hdvg.Set("UseHierarchy",BooleanValue(true));
    hdvg.Set("HelloIntervalL1", TimeValue(Seconds(helloPeriodicityL1)));
    hdvg.Set("ObservationTimeL1",IntegerValue(ObservationTimeL1));
    hdvg.Set("SendQueuePeriod",TimeValue(Seconds(SendQueuePeriod)));
    hdvh.Set("SendQueuePeriodL2",TimeValue(Seconds(SendQueuePeriodL2)));
    hdvg.Set("StohasticRebroadcastRestrictionType",IntegerValue(SRRType));
    hdvg.Set("StohasticRebroadcastRestrictionCoefficient",DoubleValue(SRRC));
    hdvg.Set("MaxHelloQueueLength",IntegerValue(MaxHelloQueueLength));
    hdvg.Set("MaxRgeoQueueLength",IntegerValue(MaxRgeoQueueLength));
    hdvg.Set("RoutingTableEntryLifeTime", TimeValue(Seconds(RoutingTableEntryLifeTime)));

    hdvg.Set("HelloIntervalL2",TimeValue(Seconds(helloPeriodicityl2)));
    hdvg.Set("ObservationTimeL2",IntegerValue(ObservationTimeL2));
    hdvg.Set("MaxClusterSize",IntegerValue(MaxClusterSize));
    hdvg.Set("MovementTreshold",DoubleValue(maxMovement));
    hdvg.Set("NoRebroadcastRange",DoubleValue(noRebroadcastRange));
    hdvg.Set("SelfIDEntryLifeTime", TimeValue(Seconds(selfIDEntryLifeTime)));*/



    switch (protocol)
    {
    case 1:
     /* list.Add (dvh, 100);*/
      protocolName = "DVH";
      break;
    case 2:
     // list.Add (dvg, 100);
      protocolName = "DVG";
      break;
    case 3:
    //  list.Add (hdvh, 100);
      protocolName = "HDVH";
      break;
    case 4:
    //  list.Add (hdvg, 100);
      protocolName = "HDVG";
      break;
    case 5:
      list.Add (olsr, 100);
      protocolName = "OLSR";
      control_port = 698;
      break;
    case 6:
      list.Add (aodv, 100);
      protocolName = "AODV";
      control_port = 654;
      break;
    case 7:
      list.Add (dsdv, 100);
      protocolName = "DSDV";
      control_port = 269;
      break;
    case 8:
     // list.Add (gpsr, 100);
      protocolName = "GPSR";
      control_port = 666;
      break;
    default:
      NS_FATAL_ERROR ("Неверный код протокола!  1 - DVH, 2 - DVG, 3 - HDVH, "
          "4 - HDVG, 5 - OLSR, 6 - AODV, 7 - DSDV, 8 - GPSR,:" << protocol);
      break;
    }

    sprintf(experiment_name, "%s.%s.%d%s.%.0f%s",
        prefix.c_str(), protocolName.c_str(), nWifis, "nodes", TotalTime, "seconds");

  }

  void
  InitInternetStack()
  {
    //Создание хелпера для установки стека интернет-протоколов на узлы
   InternetStackHelper internet;
    //Установка протокола маршрутизаци
   /* if(protocolName=="GPSR")
      {
        GpsrHelper gpsrh;
        internet.SetRoutingHelper(gpsrh);
      }
    else*/
      internet.SetRoutingHelper (list);
    //Установка стека интернет-протоколов на узлы
    internet.Install (adhocNodes);
   /* if(protocolName=="GPSR")
      {
        GpsrHelper gpsrh;
        gpsrh.Install();
      }*/
    //Создание хелпера для раздачи IP адресов узла сети
    Ipv4AddressHelper addressAdhoc;
    //Установка маски подсети - ёмкость сети 16384 адреса
    addressAdhoc.SetBase ("10.1.64.0", "255.255.192.0");
    //Присвоение адресов WiFi устройствам на узлах сети,
    //помещение результата в контейнер интерфейсов
    adhocInterfaces = addressAdhoc.Assign (adhocDevices);
  }

  void
  InitMobility()
  {

    //Создание хелпера для установки на узлы функции движения
    MobilityHelper mobilityAdhoc;
    ObjectFactory pos;
    switch (nodePosMode)
    {
    case 1:
      {
        pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
       // pos.Set ("X", RandomVariableValue (UniformVariable (0.0, sqrt(nWifis)*distance)));
char str[80];
sprintf (str, "%s%f%c", "ns3::ConstantRandomVariable[Constant=", sqrt(nWifis)*distance, ']');
 pos.Set ("X", StringValue (str));
pos.Set ("Y", StringValue (str));
        //pos.Set ("Y", RandomVariableValue (UniformVariable (0.0, sqrt(nWifis)*distance)));
        break;
      }
    case 2:
      {
        //Расстановка узлов на сетке с шагом distance по вертикали и горизонтали
        pos.SetTypeId ("ns3::GridPositionAllocator");
        pos.Set ("MinX", DoubleValue (1));
        pos.Set ("MinY", DoubleValue (1));
        pos.Set ("DeltaX", DoubleValue (distance));
        pos.Set ("DeltaY", DoubleValue (distance));
        double n = floor(sqrt(nWifis));
        pos.Set ("GridWidth", UintegerValue ((int)n));
        break;
      }
    case 3:
      {
        //Расстановка узлов в линию
        pos.SetTypeId ("ns3::GridPositionAllocator");
        pos.Set ("MinX", DoubleValue (1));
        pos.Set ("MinY", DoubleValue (1));
        pos.Set ("DeltaX", DoubleValue (distance));
        pos.Set ("DeltaY", DoubleValue (distance));
        pos.Set ("GridWidth", UintegerValue ((int)nWifis));
        break;
      }
    }

    //Создание объекта площадки симуляции
    Ptr<PositionAllocator> PositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
    //Установка параметров модели движения
    //mobilityAdhoc.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
      //  "Speed", RandomVariableValue (UniformVariable (minNodeSpeed, maxNodeSpeed)),
      //  "Pause", RandomVariableValue (ConstantVariable (nodePause)),
      //  "PositionAllocator", PointerValue (PositionAlloc));
mobilityAdhoc.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"),
        "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
        "PositionAllocator", PointerValue (PositionAlloc));
    //Установка площадки, на которой происходит симуляция
    mobilityAdhoc.SetPositionAllocator (PositionAlloc);
    //Инсталляция функции движения на узлы сети
    mobilityAdhoc.Install (adhocNodes);

  }

  void
  initTraffic()
  {

    //Создание генератора трафика
    OnOffHelper onoff1 ("ns3::UdpSocketFactory",Address ());
    //Установка длительности интервала генерации трафика

    //onoff1.SetAttribute ("OnTime", RandomVariableValue (ConstantVariable  (1)));
	
	onoff1.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"));
    //и паузы между интервалами генерации

   // onoff1.SetAttribute ("OffTime", RandomVariableValue (ConstantVariable (0)));
	onoff1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));

    //Установка размера пакетов, генерируемых приложением OnOffApplication
    onoff1.SetAttribute ("PacketSize", UintegerValue(dataPacketSize));
    //Установка скорости генерации данных приложением OnOffApplication

    onoff1.SetAttribute ("DataRate",  DataRateValue (DataRate (dataRate)));

    //Для каждого активного канала передачи в сети
    for (int i = 0; i <= nSinks - 1; i++)
      {

        Ptr<Node> reciever = adhocNodes.Get (0);
        //Получение адреса Wifi интерфейса под №1 (№0 - loopback)
        Ipv4Address rcvAddr = reciever->GetObject<Ipv4>()->GetAddress(1,0).GetLocal();

        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        //Создание UDP сокета на узле-приемнике
        Ptr<Socket> sink = Socket::CreateSocket (reciever, tid);
        //Локальный сокет
        InetSocketAddress local = InetSocketAddress (rcvAddr, dataPort);
        //Привязка адреса к созданному сокету
        sink->Bind (local);

        //Установка адреса назначения для OnOffApplication
        onoff1.SetAttribute ("Remote", AddressValue(InetSocketAddress (rcvAddr, dataPort)));
        //Установка приложения OnOff на узел-источник
        ApplicationContainer temp = onoff1.Install (adhocNodes.Get (nWifis-i-1));
        //Генерация начинается с секунды warmUpTime
        temp.Start (Seconds(warmUpTime));
        //и заканчивается за coolDownTime секунд до окончания симуляции.
        temp.Stop (Seconds (TotalTime-coolDownTime));

      }

  }

  void
  InitSniffer()
  {
    /*************** Установка снифера трафика для подсчета статистики *****************/
    //Установка сниффера на узлы
    fm = flowmonHelper.InstallAll();
  }

  void
  AnalyseAndPrint()
  {
    /*************** Анализ результатов симуляции ****************/
    //Все недоставленные пакеты, отправленные ранее,
    //чем coolDownTime секунд назад, помечаются как утерянные.
    fm->CheckForLostPackets(Time(coolDownTime));
    //Классификатор потоков IPv4
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    //Получение потоков из объекта - сниффера
    std::map<FlowId, FlowMonitor::FlowStats> flows = fm->GetFlowStats();
    //Тип протокола
    std::string proto;

    //Если в симуляции наблюдался трафик
    if (flows.size() > 0)
      {
        //Для каждого потока
        for (map< FlowId, FlowMonitor::FlowStats>::const_iterator flow = flows.begin();
            flow != flows.end(); ++flow)
          {
            //Получение объекта, содержащего служебную информацию о потоке
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow->first);
            //Получение навания протокола транспортного уровня
            switch(t.protocol)
            {
            case(6):
                                  proto = "TCP";
            break;
            case(17):
                                  proto = "UDP";
            break;
            default:
              exit(1);
            }
            //Если номера портов назначения и источника соответствуют порту для данных
            if (proto == "UDP" && t.destinationPort == dataPort)
              {
                UpdateStat(dataStat, flow->second);
              }
            else if (proto == "UDP" && t.sourcePort == control_port && t.destinationPort == control_port)
              {
                UpdateStat(ctlStatL1, flow->second);
              }
            else if (proto == "UDP" && t.destinationPort == 777)
              {
                UpdateStat(ctlStatL2, flow->second);
              }
            else
              {
                NS_LOG_UNCOND ("Ошибка! Неверный поток!" << Simulator::Now ().GetSeconds () << " с.");
              }
          }
      }

    dataStat.CalculateFinalStats();
    ctlStatL1.CalculateFinalStats();
    ctlStatL2.CalculateFinalStats();

    //Расчет коэффициента связности
   // uint p = 0;
    Ptr<Node> node;
   /* if (protocolName == "DVH" || protocolName == "DVG" ||
        protocolName == "HDVH"|| protocolName == "HDVG")
      {
        for(p = 0; p < nWifis; p++)
          {
            node = adhocNodes.Get(p);
            Ptr<ns3::dvg::RoutingProtocol> rp = node->GetObject<ns3::dvg::RoutingProtocol>();
            ns3::dvg::RoutingTable rt = rp->GetRoutingTable();
            linking += rt.GetLinkingCoeff();
          }
        linking = (double)linking/(double)nWifis;
      }*/
   /* else*/
      //Для других ротоколов не считать коэффициент связности
      linking = 0;
    //Вывод результатов симуляции в файл
    char resultFilename[100];
    sprintf(resultFilename, "%s.%s", experiment_name, "result.txt");
    //открытие файла для записи
    std::ofstream os (resultFilename, std::ios::out);
    PrintResults(os);
    os.close();
    os.open("metrics", std::ios::out);
    PrintMetrics(os);
    os.close();
    os.open("usedargs", std::ios::out);
    PrintArgs(os);
    os.close();
  };

  char *
  GetExperimentName()
  {
    return experiment_name;
  }

  bool
  GetTraceMobility()
  {
    return traceMobility;
  }

  uint32_t
  GetSimulationTime()
  {
    return TotalTime;
  }

  void
  ShowTime()
  {
    double now = Simulator::Now ().GetSeconds ();
    NS_LOG_UNCOND ("Текущее время симуляции - " << now << " с.");
    void (Experiment::*printTime)() = &Experiment::ShowTime;
    Simulator::Schedule (Seconds (1), printTime, this);

  }

  void
  TracePhyRxDropped (string ss, Ptr<const Packet> packet)
  {
    phyRxPacketDroped++;
    phyRxBytesDroped += packet->GetSize();
    double time = Simulator::Now().GetDouble()/10E8;
    int n = floor(time);
    phyRxDropedStat[n] += double(phyRxPacketDroped);
  }

  void
  TracePhyRxBegin (string ss, Ptr<const Packet> packet)
  {
    phyRxPacketBegin++;
    phyRxBytesBegin += packet->GetSize();
  }

  bool
  GetTracePHYRx()
  {
    return logPHYRxDrops;
  }

  bool
  GetTracePHYTx()
  {
    return logPHYRxDrops;
  }

  std::string GetProtocolName()
  {
    return protocolName;
  }

  void UpdateStat (TrafStat &Stat, FlowMonitor::FlowStats flowstat)
  {
    Stat.conns++;
    Stat.tx_packets += flowstat.txPackets;
    Stat.rx_packets += flowstat.rxPackets;
    Stat.lost_packets += flowstat.lostPackets;
    if (flowstat.rxPackets != 0)
      {
        Stat.mean_delay = Stat.mean_delay +
            flowstat.delaySum.GetDouble()/flowstat.rxPackets;
        Stat.route_establishing_time += flowstat.timeFirstRxPacket.GetDouble()-
            flowstat.timeFirstTxPacket.GetDouble();
        Stat.avrg_hops += (double)(flowstat.timesForwarded)/(double)(flowstat.rxPackets);
        Stat.conns_succeed ++;
      }
    // Если число принятых пакетов > 2, посчитать джиттер
    if (flowstat.rxPackets > 1)
      Stat.mean_jitter = Stat.mean_jitter +
      flowstat.jitterSum.GetDouble()/(flowstat.rxPackets-1);
  }

  void IDREQsent_count()
  {
    this->m_IDREQsent++;
  }

  void IDREQrecv_count()
  {
    this->m_IDREQrecv++;
  }

  void IDREPsent_count()
  {
    this->m_IDREPsent++;
  }

  void IDREPrecv_count()
  {
    this->m_IDREPrecv++;
  }

private:
  /*************** Общие параметры симуляции *****************/
  uint32_t protocol;      //Протокол маршрутизации
                          //(1=DVH,2=DVG,3=HDVH,4=HDVG,5=OLSR;6=AODV;7=DSDV,8=GPSR)
  uint32_t nWifis;        //Число узлов в сети
  uint32_t distance;      //Среднее расстояние между узлами по оси X и Y, м.
  uint32_t warmUpTime;    //Время начала генерации трафика, с.
  uint32_t coolDownTime;  //Время после окончания генерации трафика до конца
  //симуляции, с.
  double TotalTime;       //Общее время симуляции, с.
  uint32_t dataPort;      //UDP Порт для обмена трафиком в сети
  uint32_t nodePause;     //Продолжительность пауз между интервалами движения
  //узлов, с.
  uint32_t dataPacketSize;//Размер пакета данных, байт
  double minNodeSpeed;    //Минимальная скорость движения узлов, м/с
  double maxNodeSpeed;    //Максимальная скорость движения узлов, м/с
  double txp;             //Мощность передающего устройства, дБм.
  std::string dataRate;   //Скорость генерации трафика абонентами, бит/с.
  std::string macMode;    //Метод расширения спектра WiFi - DSSS, скорость
  //канала 1 Мбит/с.
  std::string protocolName;
  std::string prefix;
  bool traceMobility;     //Записывать ли перемещения узлов в лог?
  bool enablePcap;        //Записывать ли трафик в pcap-файл?
  bool showTime;          //Выводить ли в консоль текущее время симуляции?
  uint32_t nodePosMode;   //Расстановка узлов, 1 - случайным образом,
  //2 - равномерная на сетке
  int nSinks;                     //Число одновременно активных потоков передачи
  bool nSinksVar;                 //Устанавливать число источников в процентном соотношении
  //от числа узлов
  char experiment_name [100];     //Префикс эксперимента
  NodeContainer adhocNodes;       //Контейнер для хранения узлов сети
  Ipv4ListRoutingHelper list;     //Контейнер протоколов маршрутизации
  NetDeviceContainer adhocDevices;//Контейнер с сетевыми устройствами
  Ptr<FlowMonitor> fm;            //Сниффер, записывающий весь трафик симуляции
  FlowMonitorHelper flowmonHelper;//Хелпер для работы со сиффером
  uint32_t control_port;          //Номер служебного порта
  Ipv4InterfaceContainer adhocInterfaces;//Контейнер с сетевыми интерфейсами

  /*************** Общие параметры протоколов *****************/
  int helloPeriodicityL1;        //Периодичность рассылки HELLO-сообщений
  int ObservationTimeL1;        //Максимальная задержка для таймера опустошения
  //буфера
  double SendQueuePeriod;       //Длительность интервала опустошения кэша
  double SendQueuePeriodL2;     //Длительность интервала опустошения кэша L2
  /*Тип стохастического ограниения:
  1 - ограничение при вставке в очередь,
  2 - ограничение при рассылке содержимого очереди */
  int SRRType;
  /*Коэффициент N для стохастического
  ограничения ретрансляции. Вероятность
  ретрансляции равна N/{число соседей узла}.
  Если 0 - ограничение отсутствует*/
  double SRRC;
  int MaxHelloQueueLength;
  int MaxRgeoQueueLength;
  int helloPeriodicityl2;
  int ObservationTimeL2;
  uint32_t MaxClusterSize;           //Передвижение узла, после которого рассылаются
  // обновленные геоокоординаты
  double maxMovement;           //Радиус зоны вокруг узла, внутри которой
  //запрещена ретрансляция.
  double noRebroadcastRange;
  int selfIDEntryLifeTime;      //Время после присоединения узла к кластеру, в
  //течение которого гарантированно
  //будет находится в кластере, при этом не будет
  // приглашать в кластер других соседей
  int RoutingTableEntryLifeTime;//Время жизни записи в таблице маршрутизации

  /*************** Метрики *****************/
  bool logPHYRxDrops;
  bool logPHYTxDrops;
  uint64_t phyRxPacketDroped;
  uint64_t phyRxBytesDroped;
  uint64_t phyRxPacketBegin;
  uint64_t phyRxBytesBegin;  

  uint64_t m_IDREQsent;
  uint64_t m_IDREQrecv;
  uint64_t m_IDREPsent;
  uint64_t m_IDREPrecv;

  double *phyRxDropedStat;

  /************Результаты симуляции***********/
  TrafStat ctlStatL1;
  TrafStat ctlStatL2;
  TrafStat dataStat;

  //Доля служебного трафика
  double overhead;
  //Кожффициент связностиs
  double linking;
};
class StreamStat
{
public:
  StreamStat()
  {

  }
  void StreamModule()
  {
      ofstream streamRes;
      Experiment exp;
      streamRes.open("params1", std::ios::app);
      exp.PrintResults(streamRes);
      streamRes.close();

  void (StreamStat::*printStat)() = &StreamStat::StreamModule;
  Simulator::Schedule (Seconds (1), printStat, this);
  }

  ~StreamStat()
  {
  }
};
int
main (int argc, char *argv[])
{
  //{PyViz p;}
  //Раскомментировать для возможности работы с PyViz
  Packet::EnablePrinting ();

  Experiment exp;
  if (exp.Init(argc, argv) == -1) 
    return 0;
  StreamStat strSt;
  strSt.StreamModule();
  if(exp.GetTracePHYRx())
    {
      Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/PhyRxDrop",
          MakeCallback (&Experiment::TracePhyRxDropped, &exp));

      Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/PhyRxBegin",
          MakeCallback (&Experiment::TracePhyRxBegin, &exp));
    }

  //************** Запись перемещений узлов в файл ****************
  //Если выбрана опция записи перемещений узлов в файл
 /* ofstream mob;
  if (exp.GetTraceMobility() == true) {
      char mobtracefilename[100];
      sprintf(mobtracefilename, "%s.%s", exp.GetExperimentName(), "mob");
      //Открытие файла для записи перемещений узлов
      mob.open (mobtracefilename);
      //Вывод в файл перемещений всех узлов
      MobilityHelper::EnableAsciiAll(mob);
    }*/

  if(exp.GetProtocolName() == "DVG" || exp.GetProtocolName() == "DVH" ||
     exp.GetProtocolName() == "HDVG" || exp.GetProtocolName() == "HDVH")
    {
      Config::ConnectWithoutContext("/NodeList/*/$ns3::dvg::RoutingProtocol/IDREQsent",
                      MakeCallback (&Experiment::IDREQsent_count, &exp));
      Config::ConnectWithoutContext("/NodeList/*/$ns3::dvg::RoutingProtocol/IDREQrecv",
                      MakeCallback (&Experiment::IDREQrecv_count, &exp));
      Config::ConnectWithoutContext("/NodeList/*/$ns3::dvg::RoutingProtocol/IDREPsent",
                      MakeCallback (&Experiment::IDREPsent_count, &exp));
      Config::ConnectWithoutContext("/NodeList/*/$ns3::dvg::RoutingProtocol/IDREPrecv",
                      MakeCallback (&Experiment::IDREPrecv_count, &exp));
    }


  /* if(exp.GetProtocolName() == "DVG" || exp.GetProtocolName() == "DVH")
    {
      mkdir("Logs", 777);
      mkdir("Logs/RoutingTable", 777);
      mkdir("Logs/RoutingTable/Snapshots/", 777);
    }
  if(exp.GetProtocolName() == "HDVG" || exp.GetProtocolName() == "HDVH")
    {
      mkdir("Logs/Clustering", 777);
    }*/

  //ns3::LogComponentEnable("YansWifiChannel",LOG_DEBUG);
 // ns3::LogComponentEnable("YansWifiPhy",LOG_DEBUG);
  //ns3::LogComponentEnable("DVGRoutingProtocol",LOG_DEBUG);

  //************* Запуск симуляции ****************
  //Установка времени окончания симуляции
  Simulator::Stop (Seconds (exp.GetSimulationTime()));
  //Запуск симуляции
  Simulator::Run ();
  //Закрытие файла для записи перемещений узлов
  /*if (exp.GetTraceMobility()==true) mob.close();*/
  exp.AnalyseAndPrint();
  //Уничтожение объекта симулятора
  Simulator::Destroy ();
  return 0;
}




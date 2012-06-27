/*
    MessageWarden - an acceptation test for supercomputers
    Copyright (C) 2011, 2012  Sébastien Boisvert


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You have received a copy of the GNU General Public License
    along with this program (gpl-3.0.txt).  
	see <http://www.gnu.org/licenses/>
*/

#include <plugins/NetworkTest/NetworkTest.h>
#include <communication/Message.h>
#include <communication/mpi_tags.h>
#include <core/slave_modes.h>
#include <core/master_modes.h>
#include <cryptography/crypto.h>
#include <sstream>
#include <core/OperatingSystem.h>
#include <fstream>
#include <stdlib.h>
#include <core/statistics.h>
#include <iostream>
#include <core/ComputeCore.h>

 /* generated_automatically */
____CreateMasterModeAdapterImplementation(NetworkTest,RAY_MASTER_MODE_TEST_NETWORK); /* generated_automatically */
 /* generated_automatically */
____CreateSlaveModeAdapterImplementation(NetworkTest,RAY_SLAVE_MODE_TEST_NETWORK); /* generated_automatically */
 /* generated_automatically */
 /* generated_automatically */

____CreateMessageTagAdapterImplementation(NetworkTest,RAY_MPI_TAG_TEST_NETWORK_MESSAGE); /* generated_automatically */
____CreateMessageTagAdapterImplementation(NetworkTest,MY_TEST_MPI_TAG_STOP_AND_DIE);
using namespace std;

#define LATENCY_INFORMATION_NOT_AVAILABLE 123123123

/** initialize the NetworkTest */
void NetworkTest::constructor(int rank,int size,StaticVector*inbox,StaticVector*outbox,RingAllocator*outboxAllocator,
	string*name){

	m_name=name;
	m_inbox=inbox;
	m_outbox=outbox;
	m_initialisedNetworkTest=false;
	m_size=size;
	m_rank=rank;
	
	m_ranksFinished=0;
	m_askedToWriteFiles=false;

	int ranksPerNode=8;
	int onlineRanksPerNode=8; // default: 8

	if((m_rank % ranksPerNode) >= onlineRanksPerNode)
		m_numberOfTestMessages=0;

	m_currentTestMessage=0;
	m_sentCurrentTestMessage=false;
	m_outboxAllocator=outboxAllocator;

	/* a word is 8 bytes */
	/* MAXIMUM_MESSAGE_SIZE_IN_BYTES is 4000 per default so 
		numberOfWords must be <= 500 */
	/* this is only for the network test */
	/* default is 500 */
	m_numberOfWords=500;

	m_messagesPerRank=8000000;

	m_numberOfTestMessages=m_size*m_messagesPerRank;

	cout<<"will send "<<m_numberOfTestMessages<<endl;

	m_writeRawData=false;
	/* the seed must be different for all MPI ranks */
	srand(time(NULL)*(1+m_rank));

	// reserve space to have constant insertion
	m_sentMicroseconds.reserve(m_numberOfTestMessages);
	m_destinations.reserve(m_numberOfTestMessages);
	m_receivedMicroseconds.reserve(m_numberOfTestMessages);

	populateReference();

	if(m_rank==0){
		cout<<"Reference"<<endl;
		for(int i=0;i<m_numberOfWords;i++){
			cout<<i<<"	"<<m_reference[i]<<endl;
		}
	}
}

void NetworkTest::populateReference(){
	for(int i=0;i<m_numberOfWords;i++){

		uint64_t seed=9876589*(i+88);

		m_reference[i]=uniform_hashing_function_1_64_64(seed);
	}
}

uint64_t NetworkTest::generateOverlayData(){
	uint64_t randomNumber=rand();

	return uniform_hashing_function_1_64_64(randomNumber);
}

void NetworkTest::checkCorruption(Message*message){

	int count=message->getCount();
	MessageUnit*buffer=message->getBuffer();

	bool corrupted=false;

	if(count!=m_numberOfWords){
		corrupted=true;
	}

	int mismatches=0;

	for(int i=0;i<m_numberOfWords;i++){
		uint64_t expected=m_reference[i];
		uint64_t actual=buffer[i];

		if(expected!=actual){
			corrupted=true;
			mismatches++;
		}
	}

	if(corrupted){
		cout<<"Warning: message corruption detected on reception !"<<endl;
		cout<<" Mismatches: "<<mismatches<<endl;
		cout<<" Source: "<<message->getSource()<<endl;
		cout<<" Destination: "<<message->getDestination()<<endl;
		cout<<" Expected count: "<<m_numberOfWords<<endl;
		cout<<" Actual count: "<<count<<endl;

		cout<<"BufferIndex	Expected	Actual	Result"<<endl;
		for(int i=0;i<m_numberOfWords;i++){
			cout<<i<<"\t"<<m_reference[i]<<"\t"<<buffer[i]<<"\t";
			if(m_reference[i]==buffer[i])
				cout<<"PASS";
			else
				cout<<"FAIL";
			cout<<endl;
		}


	}

}

/** call the slave method 
 *
 * To test the network:
 * 
 * n=1000
 *
 * i=n
 * while i--
 *     destination <- random folk
 *     send full message to destination
 *     start timer
 *     receive message
 *     stop timer
 *     diff=stop-start
 *     sum+=diff
 *
 *
 * if rank==0
 *
 *   write this information to a file.
 *
 * */
void NetworkTest::call_RAY_SLAVE_MODE_TEST_NETWORK(){


	#ifdef ASSERT
	assert(m_numberOfWords*sizeof(MessageUnit) <= MAXIMUM_MESSAGE_SIZE_IN_BYTES);
	#endif

	if(m_currentTestMessage<m_numberOfTestMessages){
		if(!m_sentCurrentTestMessage){

			if(m_currentTestMessage==0)
				m_sentData=false;

			uint64_t startingTimeMicroseconds=getMicroseconds();

			/** send to a random rank */
			int destination=rand()%m_size;

			m_sentMicroseconds.push_back(startingTimeMicroseconds);
			m_destinations.push_back(destination);

			MessageUnit *message=(MessageUnit*)m_outboxAllocator->allocate(m_numberOfWords*sizeof(MessageUnit));

			for(int i=0;i<m_numberOfWords;i++){
				message[i]=m_reference[i];

			}

			//message[0]=12;

			Message aMessage(message,m_numberOfWords,destination,RAY_MPI_TAG_TEST_NETWORK_MESSAGE,m_rank);
			m_outbox->push_back(aMessage);
			m_sentCurrentTestMessage=true;

			#ifdef I_MUST_DEBUG
			cout<<m_rank<<" sends RAY_MPI_TAG_TEST_NETWORK_MESSAGE to "<<destination<<endl;
			#endif

		}else if(m_inbox->size()>0 && m_inbox->at(0)->getTag()==RAY_MPI_TAG_TEST_NETWORK_MESSAGE_REPLY){
			uint64_t endingMicroSeconds=getMicroseconds();
			
			Message*messageObject=m_inbox->at(0);

			checkCorruption(messageObject);

			m_receivedMicroseconds.push_back(endingMicroSeconds);

			m_sentCurrentTestMessage=false;

			if(m_currentTestMessage % 1000 == 0){
				cout<<"Rank "<<m_rank<<" is testing the network ["<<m_currentTestMessage<<"/";
				cout<<m_numberOfTestMessages<<"]"<<endl;
			}

			m_currentTestMessage++;
		}

	}else if(!m_sentData){


		cout<<"Rank "<<m_rank<<" is testing the network ["<<m_currentTestMessage<<"/";
		cout<<m_numberOfTestMessages<<"]"<<endl;

		// we finished gathering data.
		// now we compute the mode for the latency
		// TODO: this should probably done after everyone has finished

		int latency=getModeLatency();

		cout<<"Rank "<<m_rank<<": mode latency for "<<(*m_name)<<" when requesting a reply for a message of "<<sizeof(MessageUnit)*m_numberOfWords<<" bytes is "<<latency<<" microseconds (10^-6 seconds)"<<endl;

		MessageUnit*message=(MessageUnit*)m_outboxAllocator->allocate(MAXIMUM_MESSAGE_SIZE_IN_BYTES);
		message[0]=latency;
		char*destination=(char*)(message+1);
		strcpy(destination,m_name->c_str());
		Message aMessage(message,MAXIMUM_MESSAGE_SIZE_IN_BYTES/sizeof(MessageUnit),
			MASTER_RANK,RAY_MPI_TAG_TEST_NETWORK_REPLY,m_rank);
		m_outbox->push_back(aMessage);

		m_sentData=true;

		m_gotResponse=false;

	}else if(m_inbox->hasMessage(RAY_MPI_TAG_TEST_NETWORK_REPLY_REPLY)){
		m_gotResponse=true;

	}else if(m_gotResponse && m_inbox->hasMessage(RAY_MPI_TAG_TEST_NETWORK_WRITE_DATA)){
		// we only write the files, if any, when everyone is done with it
		// otherwise, the measured latency would be higher...
		writeData();

		m_switchMan->closeSlaveModeLocally(m_outbox,m_rank);
	}
}

void NetworkTest::writeData(){
	/* nothing to write */
	if(m_sentMicroseconds.size() == 0)
		return;

	if(m_writeRawData){
		ostringstream file;
		file<<m_prefix;
		file<<"Rank"<<m_rank<<".NetworkTestData.txt";
	
		cout<<"Rank "<<m_rank<<" is writing "<<file.str()<<" (-write-network-test-raw-data)"<<endl;

		ofstream f(file.str().c_str());

		f<<"# tag for test messages: RAY_MPI_TAG_TEST_NETWORK_MESSAGE"<<endl;
		f<<"# tag for reply messages: RAY_MPI_TAG_TEST_NETWORK_MESSAGE_REPLY"<<endl;
		f<<"# number of words per message: "<<m_numberOfWords<<endl;
		f<<"# word size in bytes: "<<sizeof(MessageUnit)<<endl;
		f<<"# number of bytes for test messages: "<<m_numberOfWords*sizeof(MessageUnit)<<endl;
		f<<"# number of bytes for reply messages: 0"<<endl;
		f<<"# number of test messages: "<<m_numberOfTestMessages<<endl;
		f<<"# mode latency measured in microseconds: "<<getModeLatency()<<endl;
		f<<"# next line contains column names"<<endl;
		f<<"# TestMessage SourceRank DestinationRank QueryTimeInMicroseconds ReplyTimeInMicroseconds Latency MessagesSentToDestination"<<endl;
	
		#ifdef ASSERT
		assert(m_sentMicroseconds.size() == m_destinations.size());
		assert(m_sentMicroseconds.size() == m_receivedMicroseconds.size());
		assert((int) m_sentMicroseconds.size() == m_numberOfTestMessages);
		#endif

		map<int,int> counters;

		for(int i=0;i<(int)m_sentMicroseconds.size();i++){
			uint64_t time1=m_sentMicroseconds[i];
			uint64_t time2=m_receivedMicroseconds[i];
			int destination=m_destinations[i];
			counters[destination] ++ ;
			f<<i<<"	"<<"	"<<m_rank<<"	"<<destination<<"	"<<time1<<"	"<<time2<<"	"<<time2-time1<<"	"<<counters[destination]<<endl;
		}

		f.close();

		ostringstream file2;
		file2<<m_prefix;
		file2<<"Rank"<<m_rank<<".NetworkTestDataCount.txt";
		
		ofstream f2(file2.str().c_str());
		f2<<"# DestinationRank	MessagesSentToDestination"<<endl;
		for(map<int,int>::iterator i=counters.begin();i!=counters.end();i++){
			f2<<i->first<<"	"<<i->second<<endl;
		}

		f2.close();
	}

	m_destinations.clear();
	m_sentMicroseconds.clear();
	m_receivedMicroseconds.clear();
}

/** call the master method */
void NetworkTest::call_RAY_MASTER_MODE_TEST_NETWORK (){
	if(!m_initialisedNetworkTest){
		cout<<"Rank 0: testing the network, please wait..."<<endl;
		cout<<endl;

		m_switchMan->openMasterMode(m_outbox,m_rank);

		m_initialisedNetworkTest=true;
	}else if(m_inbox->size()>0&&(*m_inbox)[0]->getTag()==RAY_MPI_TAG_TEST_NETWORK_REPLY){
		int rank=m_inbox->at(0)->getSource();
		int latency=m_inbox->at(0)->getBuffer()[0];

		MessageUnit*buffer=m_inbox->at(0)->getBuffer();

		char*name=(char*)(buffer+1);
		string stringName=name;
		m_names[rank]=stringName;
		m_latencies[rank]=latency;

		m_switchMan->sendEmptyMessage(m_outbox,m_rank,rank,RAY_MPI_TAG_TEST_NETWORK_REPLY_REPLY);

		m_ranksFinished++;

	}else if(m_switchMan->allRanksAreReady() && !m_finished){

		ostringstream file;
		file<<m_prefix;
		file<<"NetworkTest.txt";
		ofstream f(file.str().c_str());
		f<<"# average latency in microseconds (10^-6 seconds) when requesting a reply for a message of "<<sizeof(MessageUnit)*m_numberOfWords<<" bytes"<<endl;
		f<<"# MessagePassingInterfaceRank\tName\tModeLatencyInMicroseconds\tNumberOfTestMessages"<<endl;

		vector<int> latencies;
		for(int i=0;i<m_size;i++){
			int latency=m_latencies[i];
			if(latency!=LATENCY_INFORMATION_NOT_AVAILABLE)
				latencies.push_back(latency);
		}

		f<<"# AverageForAllRanks: "<<getAverage(&latencies)<<endl;
		f<<"# StandardDeviation: "<<getStandardDeviation(&latencies)<<endl;

		for(int i=0;i<m_size;i++){
			int latency=m_latencies[i];
			if(latency==LATENCY_INFORMATION_NOT_AVAILABLE){
				f<<i<<"\t"<<m_names[i]<<"\tLATENCY_INFORMATION_NOT_AVAILABLE";
				f<<"	"<<m_numberOfTestMessages<<endl;
			}else{
				f<<i<<"\t"<<m_names[i]<<"\t"<<latency;
				f<<"	"<<m_numberOfTestMessages<<endl;
			}
		}
		f.close();
		m_latencies.clear();

		m_switchMan->closeMasterMode();

		cout<<endl;
		cout<<"Rank "<<m_rank<<" wrote "<<file.str()<<endl;
		cout<<endl;
		
		m_finished=true;

		m_core->sendEmptyMessageToAll(MY_TEST_MPI_TAG_STOP_AND_DIE);

	}else if(m_ranksFinished==m_size && !m_askedToWriteFiles){
		
		m_switchMan->sendToAll(m_outbox,m_rank,RAY_MPI_TAG_TEST_NETWORK_WRITE_DATA);

		m_askedToWriteFiles=true;
	}
}

void NetworkTest::call_MY_TEST_MPI_TAG_STOP_AND_DIE(Message*message){

	cout<<"rank "<<m_core->getMessagesHandler()->getRank()<<" received message MY_TEST_MPI_TAG_STOP_AND_DIE, this kills the batman"<<endl;

	m_core->stop();
}



/* we reply with an empty message */
void NetworkTest::call_RAY_MPI_TAG_TEST_NETWORK_MESSAGE(Message*message){

	checkCorruption(message);

	int count=message->getCount();
	MessageUnit*buffer=message->getBuffer();

	MessageUnit *reply=(MessageUnit*)m_outboxAllocator->allocate(count*sizeof(MessageUnit));

	for(int i=0;i<count;i++){
		reply[i]=m_reference[i];
	}

	Message aMessage(reply,count,message->getSource(),RAY_MPI_TAG_TEST_NETWORK_MESSAGE_REPLY,m_rank);
	m_outbox->push_back(aMessage);
}

int NetworkTest::getModeLatency(){
	map<int,int> data;
	int maxLatency=-1;

	for(int i=0;i<(int)m_receivedMicroseconds.size();i++){
		int latency=m_receivedMicroseconds[i]-m_sentMicroseconds[i];
		data[latency]++;

		if(data.count(maxLatency)==0 || data[latency] > data[maxLatency])
			maxLatency=latency;
	}
	
	return maxLatency;
}

void NetworkTest::setSwitchMan(SwitchMan*a){
	m_switchMan=a;
}

void NetworkTest::registerPlugin(ComputeCore*core){

	PluginHandle plugin = core->allocatePluginHandle();
	m_plugin=plugin;

	core->setPluginName(plugin,"NetworkTest");
	core->setPluginDescription(plugin,"This is a plugin used to test the network latency.");
	core->setPluginAuthors(plugin,"Sébastien Boisvert");
	core->setPluginLicense(plugin,"GNU General Public License version 3");

	RAY_SLAVE_MODE_TEST_NETWORK=core->allocateSlaveModeHandle(plugin);
	m_adapter_RAY_SLAVE_MODE_TEST_NETWORK.setObject(this);
	core->setSlaveModeObjectHandler(plugin,RAY_SLAVE_MODE_TEST_NETWORK, &m_adapter_RAY_SLAVE_MODE_TEST_NETWORK);
	core->setSlaveModeSymbol(plugin,RAY_SLAVE_MODE_TEST_NETWORK,"RAY_SLAVE_MODE_TEST_NETWORK");

	RAY_MASTER_MODE_TEST_NETWORK=core->allocateMasterModeHandle(plugin);
	m_adapter_RAY_MASTER_MODE_TEST_NETWORK.setObject(this);
	core->setMasterModeObjectHandler(plugin,RAY_MASTER_MODE_TEST_NETWORK, &m_adapter_RAY_MASTER_MODE_TEST_NETWORK);
	core->setMasterModeSymbol(plugin,RAY_MASTER_MODE_TEST_NETWORK,"RAY_MASTER_MODE_TEST_NETWORK");

	RAY_MPI_TAG_TEST_NETWORK=core->allocateMessageTagHandle(plugin);
	core->setMessageTagSymbol(plugin,RAY_MPI_TAG_TEST_NETWORK,"RAY_MPI_TAG_TEST_NETWORK");

	RAY_MPI_TAG_TEST_NETWORK_MESSAGE=core->allocateMessageTagHandle(plugin);
	core->setMessageTagSymbol(plugin,RAY_MPI_TAG_TEST_NETWORK_MESSAGE,"RAY_MPI_TAG_TEST_NETWORK_MESSAGE");
	m_adapter_RAY_MPI_TAG_TEST_NETWORK_MESSAGE.setObject(this);
	core->setMessageTagObjectHandler(plugin,RAY_MPI_TAG_TEST_NETWORK_MESSAGE,&m_adapter_RAY_MPI_TAG_TEST_NETWORK_MESSAGE);

	RAY_MPI_TAG_TEST_NETWORK_MESSAGE_REPLY=core->allocateMessageTagHandle(plugin);
	core->setMessageTagSymbol(plugin,RAY_MPI_TAG_TEST_NETWORK_MESSAGE_REPLY,"RAY_MPI_TAG_TEST_NETWORK_MESSAGE_REPLY");

	RAY_MPI_TAG_TEST_NETWORK_REPLY=core->allocateMessageTagHandle(plugin);
	core->setMessageTagSymbol(plugin,RAY_MPI_TAG_TEST_NETWORK_REPLY,"RAY_MPI_TAG_TEST_NETWORK_REPLY");

	RAY_MPI_TAG_TEST_NETWORK_REPLY_REPLY=core->allocateMessageTagHandle(plugin);
	core->setMessageTagSymbol(plugin,RAY_MPI_TAG_TEST_NETWORK_REPLY_REPLY,"RAY_MPI_TAG_TEST_NETWORK_REPLY_REPLY");

	RAY_MPI_TAG_TEST_NETWORK_WRITE_DATA=core->allocateMessageTagHandle(plugin);
	core->setMessageTagSymbol(plugin,RAY_MPI_TAG_TEST_NETWORK_WRITE_DATA,"RAY_MPI_TAG_TEST_NETWORK_WRITE_DATA");


	MY_TEST_MPI_TAG_STOP_AND_DIE=core->allocateMessageTagHandle(m_plugin);
	m_adapter_MY_TEST_MPI_TAG_STOP_AND_DIE.setObject(this);
	core->setMessageTagObjectHandler(m_plugin,MY_TEST_MPI_TAG_STOP_AND_DIE,&m_adapter_MY_TEST_MPI_TAG_STOP_AND_DIE);

}

void NetworkTest::resolveSymbols(ComputeCore*core){
	RAY_SLAVE_MODE_TEST_NETWORK=core->getSlaveModeFromSymbol(m_plugin,"RAY_SLAVE_MODE_TEST_NETWORK");

	RAY_MASTER_MODE_TEST_NETWORK=core->getMasterModeFromSymbol(m_plugin,"RAY_MASTER_MODE_TEST_NETWORK");

	RAY_MPI_TAG_TEST_NETWORK_MESSAGE=core->getMessageTagFromSymbol(m_plugin,"RAY_MPI_TAG_TEST_NETWORK_MESSAGE");
	RAY_MPI_TAG_TEST_NETWORK_MESSAGE_REPLY=core->getMessageTagFromSymbol(m_plugin,"RAY_MPI_TAG_TEST_NETWORK_MESSAGE_REPLY");
	RAY_MPI_TAG_TEST_NETWORK_REPLY=core->getMessageTagFromSymbol(m_plugin,"RAY_MPI_TAG_TEST_NETWORK_REPLY");
	RAY_MPI_TAG_TEST_NETWORK_REPLY_REPLY=core->getMessageTagFromSymbol(m_plugin,"RAY_MPI_TAG_TEST_NETWORK_REPLY_REPLY");
	RAY_MPI_TAG_TEST_NETWORK_WRITE_DATA=core->getMessageTagFromSymbol(m_plugin,"RAY_MPI_TAG_TEST_NETWORK_WRITE_DATA");

	RAY_MPI_TAG_TEST_NETWORK=core->getMessageTagFromSymbol(m_plugin,"RAY_MPI_TAG_TEST_NETWORK");

	core->setMasterModeToMessageTagSwitch(m_plugin,RAY_MASTER_MODE_TEST_NETWORK, RAY_MPI_TAG_TEST_NETWORK);

	core->setMessageTagToSlaveModeSwitch(m_plugin,RAY_MPI_TAG_TEST_NETWORK,                 RAY_SLAVE_MODE_TEST_NETWORK);


	core->setFirstMasterMode(m_plugin,RAY_MASTER_MODE_TEST_NETWORK);

	m_prefix="MessageWarden.";

	constructor(core->getMessagesHandler()->getRank(),
		core->getMessagesHandler()->getSize(),
		core->getInbox(),
		core->getOutbox(),
		core->getOutboxAllocator(),
		core->getMessagesHandler()->getName()
	);

	setSwitchMan(core->getSwitchMan());

	m_core=core;
	
	#ifdef I_MUST_DEBUG
	core->showCommunicationEvents();
	#endif
	
	m_finished=false;
}

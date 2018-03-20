#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/soundcard.h>
#include <alsa/asoundlib.h>
#include<thread>
#include <sys/stat.h>
#include <fstream>
#include "Talker.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "qisr.h"
#include "msp_cmn.h"
#include "msp_errors.h"

//#include "../include/Talker.h"
//#include "../include/rapidjson/document.h"
//#include "../include/rapidjson/writer.h"
//#include "../include/rapidjson/stringbuffer.h"
//#include "../include/qisr.h"
//#include "../include/msp_cmn.h"
//#include "../include/msp_errors.h"

using namespace std;
using namespace rapidjson;

Talker::Talker(){}
Talker::~Talker(){}

void sig_handler(int s){
    exit(0);
}

//读取两个json文件,dictionary.txt是对话库，greeting.txt是问候语
//传入的路径为assets的路径
int Talker::init(string basepath){
    string json_dictionary;
    string json_greeting;
    basePath = basepath;

    string file_dict = basepath+"/assets/dictionary.txt";
    cout<<"basePath:  "<<basepath<<endl;

    ifstream infile;
    infile.open(file_dict.data());
    if(!infile.is_open()){
        cout<<"Fail to find expected Json file"<<endl;
        return -1;
    }
    char c;
    while(!infile.eof()){
        infile>>c;
        json_dictionary.append(1,c);
    }
    json_dictionary.erase(json_dictionary.size()-1,1);
    infile.close();

    string file_greet = basepath+"/assets/greeting.txt";

    ifstream infile2;
    infile2.open(file_greet.data());
    if(!infile2.is_open()){
         cout<<"Fail to find expected Json file"<<endl;
        return -1;
    }
    char c2;
    while(!infile2.eof()){
        infile2>>c2;
        json_greeting.append(1,c2);
    }
    json_greeting.erase(json_greeting.size()-1,1);
    infile2.close();

    dictionaryDoc.Parse(json_dictionary.c_str());
    if(dictionaryDoc.HasParseError()){
        cout<<"Parsing Json Error  ---  Invalid json file :"<<file_dict<<endl;
        return -1;
    }
    greetingDoc.Parse(json_greeting.c_str());
    if(greetingDoc.HasParseError()){
        cout<<"Parsing Json Error  ---  Invalid json file :"<<file_greet<<endl;
        return -1;
    }else{
        cout<<"Parse  json success"<<endl;
    }
//    cout<<json_dictionary<<endl;
//    cout<<json_greeting<<endl;

    //如果是离线语音识别，这里不需要上传热词
//    while(uploadHotWords()==-1){
//        signal(SIGINT,sig_handler);
//        sleep(5);
//        cout<<"Upload hot words failed. Retrying"<<endl;
//    }
    return 0;
}

//根据传入的文本进行语音对话或根据文字查找前往目标，
//chatMsg是对话的文本形式(由讯飞语音识别得到)
int Talker::chat(string  chatMsg,on_play_finished callback) {
    Value& json = dictionaryDoc["dictionary"];
    cout<<chatMsg<<" ,length:"<<chatMsg.length()<<endl;
    if(chatMsg.length()<=2){
        callback(REQUEST_AUDIO_UNMATCH,"");
        return -1;
    }

    if(json.IsArray()){
        for(int i =0;i<json.Size();i++){
            Value& v = json[i];
            if(v.HasMember("keyword")&&v.HasMember("answer")){
                Value& words = v["keyword"];
                Value& flagValue = v["flag"];
                int flag = flagValue.GetInt();
                bool isSearch = false;
                if(flag==0){
                    for(int j=0;j<words.Size();j++){
                        if(chatMsg.find(words[j].GetString())!=-1){
                            Value& isAudio = v["isAudio"];
                            if(isAudio.GetInt()==0){
                                Value& answer = v["answer"];
                                goalName = answer.GetString();
                                callback(REQUEST_CHAT_QUERY_GOAL,answer.GetString());
                                return 0;
                            }else{
                                string audiofile = basePath+"/assets/"+v["answer"].GetString();
                                play((char*)audiofile.c_str(),REQUEST_CHAT,callback);
                                return 0;
                            }

                        }
                    }
                }else if(flag ==1){
                    for(int j=0;j<words.Size();j++){
                        if(chatMsg.find(words[j].GetString())!=-1){
                            if(j==words.Size()-1){
                                string audiofile = basePath+"/assets/"+v["answer"].GetString();
                                play((char*)audiofile.c_str(),REQUEST_CHAT,callback);
                                return 0;
                            }
                        }else{
                            break;
                        }
                    }
                }

            }else{
                cout<<"Error occurs when parsing the "<<i<<" object from dictionary"<<endl;
            }
        }

//        string audiofile = basePath+"/assets/wav/hard.wav";
//        play((char*)audiofile.c_str(),REQUEST_AUDIO_UNMATCH,callback);
        return 0;

    }else{
        cout<<"Cannot get dictionary from json file"<<endl;
    }
    return -1;


}


//人脸识别后，播放问候音频 ,传入的name为员工姓名的拼音形式
int Talker::greetByName(string goalname,on_play_finished callback){
    Value& arr = greetingDoc["greeting"];
    if(arr.IsArray()){
        for(int i=0;i<arr.Size();i++){
            Value& v = arr[i];
            Value& vname = v["name"];
            if(goalname.find(vname.GetString())!=-1){
                Value& vaudio =  v["greet_audio"];
                string filename = basePath+"/assets/"+vaudio.GetString();
                play((char*)filename.c_str(),REQUEST_GREET_STAFF,callback);
                return 0;
            }
        }
    }else{
        cout<<"Parse error"<<endl;
        return -1;
    }

    //如果没有检索到合适的值，则最后按照游客处理
    string filename = basePath+"/assets/wav/greet_visitor.wav";
    play((char*)filename.c_str(),REQUEST_GREET_VISITOR,callback);

    return 0;
}

//当到达员工位置时，通知员工对访客进行接待,传入的name为员工姓名的拼音形式
int Talker::informWhenReachGoal(string name,on_play_finished callback){
    Value& arr = greetingDoc["greeting"];
    if(arr.IsArray()){
        for(int i=0;i<arr.Size();i++){
            Value& v = arr[i];
            Value& vname = v["name"];
            if(name.find(vname.GetString())!=-1){
                Value& vaudio =  v["reach_audio"];
                string filename = basePath+"/assets/"+vaudio.GetString();
                play((char*)filename.c_str(),REQUEST_REACH_GOAL,callback);
                return 0;
            }
        }
    }else{
        cout<<"Parse error"<<endl;
        return -1;
    }
}


//播放一个指定路径的文件
int Talker::play(char* file,int requestCode,on_play_finished  callback){
//    cout<<"Talker  talk()  tid: "<<this_thread::get_id()<<endl;
    cout<<"Start talking.......   "<<file<<endl;

    FILE*   fp  = NULL;
    int rc;
    int ret;
    int size;
    snd_pcm_t* handle; //PCI设备句柄
    snd_pcm_hw_params_t* params;//硬件信息和PCM流配置
    unsigned int val;
    int dir=0;
    snd_pcm_uframes_t frames;
    char *buffer;

    unsigned char ch[100]; //用来存储wav文件的头信息
    wave_pcm_hdr wave_header;
    fp=fopen(file,"rb");
    if(!fp){
        perror("Cannot find audio file\n");
    }

     fread(&wave_header,1,sizeof(wave_header),fp);
     int channels=wave_header.channels;
     int frequency=wave_header.samples_per_sec;
     int bit=wave_header.bits_per_sample;
     int datablock=wave_header.block_align;

     struct stat statbuf;
     stat(file,&statbuf);
     int filesize = statbuf.st_size;
     //the audio file is  256Kbps   , so the duration is : filesize(KB)*8/256K
     int duration_second = filesize*8.0/256000;

    rc=snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if(rc<0){
        perror("\nOpen PCM device failed:");
        exit(1);
    }

    snd_pcm_hw_params_alloca(&params); //分配params结构体
    if(rc<0)
    {
        perror("\nSnd_pcm_hw_params_alloca:");
        exit(1);
    }
    rc=snd_pcm_hw_params_any(handle, params);//初始化params
    if(rc<0) {
        perror("\nSnd_pcm_hw_params_any:");
        exit(1);
    }
    rc=snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED); //初始化访问权限
    if(rc<0){
        perror("\nSed_pcm_hw_set_access:");
        exit(1);

    }

    //采样位数
    switch(bit/8)
    {
    case 1:snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_U8);
        break ;
    case 2:snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
        break ;
    case 3:snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S24_LE);
        break ;

    }
    rc=snd_pcm_hw_params_set_channels(handle, params, channels); //设置声道,1表示单声道，2表示立体声
    if(rc<0)
    {
        perror("\nSnd_pcm_hw_params_set_channels:");
        exit(1);
    }
    val = frequency;
    rc=snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir); //设置频率
    if(rc<0)
    {
        perror("\nSnd_pcm_hw_params_set_rate_near:");
        exit(1);
    }

    rc = snd_pcm_hw_params(handle, params);
    if(rc<0)
    {
        perror("\nSnd_pcm_hw_params: ");
        exit(1);
    }

    /*获取周期长度*/
    rc=snd_pcm_hw_params_get_period_size(params, &frames, &dir);


    if(rc<0)
    {
        perror("\nSnd_pcm_hw_params_get_period_size:");
        exit(1);
    }

    size = frames * datablock; //代表数据块长度  datablock  : 2
//    cout<<"dataBlock: "<<datablock<<endl;
    buffer =(char*)malloc(size);
    fseek(fp,58,SEEK_SET); //定位歌曲到数据区


    while (1){
        memset(buffer,0,sizeof(buffer));
        ret = fread(buffer, 1, size, fp);
        if(ret==0){
            break;
        }
         // 写音频数据到PCM设备
        ret = snd_pcm_writei(handle, buffer, frames);
    }
    //cout<<"duration_second:"<<duration_second<<endl;
    int r = snd_pcm_drain(handle);
    callback(requestCode,"Playing Done");

    snd_pcm_close(handle);
    free(buffer);
    return 0;
}

//上传热词
int Talker::uploadHotWords(){
    string login_parameters = "appid = 5a52e95f, work_dir = "+basePath+"/assets";

    int r = MSPLogin(NULL,NULL,login_parameters.c_str());
    if(MSP_SUCCESS !=r){
        cout<<"Login Failed ,Error code :"<<r<<endl;
    }

    char* userwords =	 NULL;
    size_t	 len =	0;
    size_t	 read_len =0;
    FILE* fp	 =	NULL;
    int ret =	-1;
    string file = basePath+"/assets/hotwords.txt";
    fp = fopen(file.c_str(), "rb");
    if (NULL == fp){
        cout<<"open [hotwords.txt] failed! "<<endl;
    }
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    userwords = (char*)malloc(len + 1);
    if (NULL == userwords){
        cout<<"out of memory! "<<endl;
        return -1;
    }
    read_len = fread((void*)userwords, 1, len, fp);
    if (read_len != len){
        cout<<"read [hotwords.txt] failed!"<<endl;
        return -1;
    }
    userwords[len] = '\0';
    MSPUploadData("userwords", userwords, len, "sub = uup, dtt = userword", &ret);

    if (MSP_SUCCESS != ret){
        cout<<"MSPUploadData failed ! errorCode:"<< ret<<endl;
        if(ret==10114){
            string audio_file = basePath+"/assets/wav/networktimeout.wav";
            MSPLogout();
            play((char*)audio_file.c_str(),REQUEST_SIMPLE_PLAY,simple_call_back);
        }
        return -1;
    }
   MSPLogout();
    return 0;
}








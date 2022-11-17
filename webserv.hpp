#include <vector>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <stack>
#include <unistd.h>
#include <algorithm>
#include "server.hpp"
#include "client.hpp"
#include <sys/socket.h> //socket
// #include <sys/un.h>
#include <sys/event.h> //kqueue
#include <stdexcept>
#include <sstream>
#include "util.hpp"
#include "IO_manager.hpp"

#define DETECT_SIZE 1024; //한 횟차에 처리할 최대 이벤트 갯수.
#define FAIL        -1; //실패를 의미하는 매크로.
#define RECV_ALL    1; //모두 수신 받음을 의미.

class Webserv
{
private:
    std::vector<Server> _server_list;
    std::vector<Client> _client_list;

public:
    Webserv(/* args */){};
    ~Webserv(){};
    std::vector<Server> get_server_list()
    {
        return this->_server_list;
    };
    void set_server_list(Server new_server)
    {
        this->_server_list.push_back(new_server);
    };
    std::vector<Client> get_client_list()
    {
        return this->_client_list;
    };
    void set_client_list(Client new_client)
    {
        this->_client_list.push_back(new_client);
    };

    /*
        - check bracket
        - 일단 정상적인 값이 들어온다는 전제
        - stack
        - 나올수 있는 예시들 listup하고서 find한다. (listen, root, index, location)
        - variable nameing rule check
            - start with _, alphabet
            - no start with digit
        - key값 중복 여부 확인 필요

    */
    bool    check_config_validation(std::ifstream &config_fd)
    {
        /*  체크사항
            노션 - 결정 사항 (11/16) 페이지에 정리해둠. 
        */
        std::string line;
        std::stack<std::string> bracket_stack;

        while (!config_fd.eof())
        {
            std::vector<std::string> split_result;
            getline(config_fd, line);
            int semicolon_cnt = util::count_semicolon(line);
            if (semicolon_cnt >= 2)
                return (false);
            // split_result.clear();
            split_result = util::ft_split(line, ";");
            if (split_result.size() == 0)
                continue;
            split_result = util::ft_split(split_result[0], "\t \n");
            if (split_result.size() == 0)
                continue;
            if (split_result[0] == "server")
            {
                // 가능한 유일한 입력 : server {
                if (semicolon_cnt != 0)
                    return (false);
                    
                if (split_result.size() != 2 || split_result[1] != "{")
                    return (false);
                bracket_stack.push("{"); // 여는 괄호를 넣어준다.
            }
            else if (split_result[0] == "listen")
            {
                // 가능한 입력 : listen 80; listen 70 ;
                if (semicolon_cnt != 1 || split_result.size() != 2) // 
                    return (false);
                if (util::is_numeric(split_result[1]) == false)
                    return (false);
            }
            else if (split_result[0] == "server_name")
            {
                // 가능한 입력 : server_name lalala; server_name lalala ;
                if (semicolon_cnt != 1 || split_result.size() != 2)
                    return (false);
            }
            else if (split_result[0] == "root")
            {
                // 가능한 입력 : root path; root path ;
                if (semicolon_cnt != 1 || split_result.size() != 2)
                    return (false);
            }
            else if (split_result[0] == "index")
            {
                if (semicolon_cnt != 1 || split_result.size() < 2)
                    return (false);
            }
            else if (split_result[0] == "default_error_pages")
            {
                if (semicolon_cnt != 1 || split_result.size() != 3)
                    return (false);
                if (util::is_numeric(split_result[1]) == false)
                    return (false);
            }
            else if (split_result[0] == "autoindex")
            {
                if (semicolon_cnt != 1 || split_result.size() != 2)
                    return (false);
                if (split_result[1] != "on" && split_result[1] != "off")
                    return (false);
            }
            else if (split_result[0] == "client_max_body_size")
            {
                if (semicolon_cnt != 1 || split_result.size() != 2)
                    return (false);
                if (util::is_numeric(split_result[1]) == false)
                    return (false);
            }
            else if (split_result[0] == "cgi")
            {
                if (semicolon_cnt != 1 || split_result.size() != 3)
                    return (false);
                if (split_result[1] != "none" && split_result[1] != ".py" && split_result[1] != ".php")
                    return (false);
            }
            else if (split_result[0] == "location")
            {
                if (semicolon_cnt != 0 || split_result.size() != 3)
                    return (false);
                if (split_result[2] != "{")
                    return (false);
                bracket_stack.push("{");
                // 계속 읽어준다.
                while (!config_fd.eof())
                {
                    getline(config_fd, line);
                    semicolon_cnt = util::count_semicolon(line);
                    if (semicolon_cnt > 1)
                        return (false);
                    split_result = util::ft_split(line, ";");
                    split_result = util::ft_split(split_result[0], "\t \n");
                    if (split_result[0] == "}")
                    {
                        if (semicolon_cnt != 0)
                            return (false);
                        if (bracket_stack.empty())
                            return (false);
                        bracket_stack.pop();
                        break;
                    }

                    if (split_result[0] != "root" && split_result[0] != "index")
                        return (false);
                    if (split_result.size() != 2)
                        return (false);
                }
            }
            else if (split_result[0] == "}")
            {
                if (semicolon_cnt != 0)
                    return (false);
                if (bracket_stack.empty() == true)
                    return (false);
                bracket_stack.pop();
            }
            else 
            {
                // error
                return (false);
            }
        }
        if (!bracket_stack.empty())
            return (false);
        return (true);
    }

    //conf파일을 인자를 받아 파싱 메소드.
    bool    parsing(std::string conf_file)
    {
        std::ifstream config_fd;
    	std::string line;

    	config_fd.open(conf_file);
        // ifstream 실패했을 경우 -> good() false
        if (config_fd.good() == false)
             return (false);
        if (check_config_validation(config_fd) == false)
        {
            config_fd.close();
             return (false);
        }
        config_fd.close();
        // 유효성 확인된 경우 파싱 시작
        config_fd.open(conf_file);
	    while (!config_fd.eof())
	    {
		    std::vector<std::string> split_result;
            std::stringstream ss;
            getline(config_fd, line);
            split_result = util::ft_split(line, "\t \n");
            if (split_result.size() == 0)
                continue;
		    //함수 호출, 정규표현식은 없다.
		    //첫줄이 server 로 시작하면 server 리스트에 1개 추가
            if (split_result[0] == "server")
            {
                Server new_server;
                set_server_list(new_server);
            }
            else if (split_result[0] == "listen")
            {
                util::remove_last_semicolon(split_result[1]);
                ss.str(split_result[1]);
                int port;
                ss >> port; // 이 경우 a80 은 0으로, 80a는 80으로 파싱됨 -> 유효성 검사 부분에서 처리 필요함.
                for(int i = 0; i < _server_list.size() - 1; i++)
                {
                    if (_server_list[i].port == port)
                    {
                        return (false);
                    }
                }
                 _server_list.back().port = port;
            }
            else if (split_result[0] == "server_name")
            {
                util::remove_last_semicolon(split_result[1]);
                _server_list.back().server_name = split_result[1];
            }
            else if (split_result[0] == "root")
            {
                util::remove_last_semicolon(split_result[1]);
                _server_list.back().root = split_result[1];
            }
            else if (split_result[0] == "index")
            {
                for (int i = 1; i < split_result.size(); i++)
                {
                    if (split_result[i] == ";")
                        break;
                    util::remove_last_semicolon(split_result[i]);
                    _server_list.back().index.push_back(split_result[i]);
                }
            }
            else if (split_result[0] == "autoindex")
            {
                util::remove_last_semicolon(split_result[1]);
                _server_list.back().autoindex = (split_result[1] == "on");
            }
            else if (split_result[0] == "client_max_body_size")
            {
                util::remove_last_semicolon(split_result[1]);
                ss.str(split_result[1]);
                ss >> _server_list.back().client_max_body_size;
            }
            else if (split_result[0] == "error_page")
            {
                int status_code;
                util::remove_last_semicolon(split_result[1]);
                ss.str(split_result[1]);
                ss >> status_code;
                util::remove_last_semicolon(split_result[2]);
                _server_list.back().default_error_pages.insert(std::make_pair(status_code, split_result[2]));
            }
            else if (split_result[0] == "cgi")
            {
                if (split_result.size() >= 3 && (split_result[1] == ".py" || split_result[1] == ".php"  || split_result[1] == "none"))
                {
                    _server_list.back().cgi_map.insert(std::make_pair(split_result[1],split_result[2]));
                }
            }
            else if (split_result[0] == "location")
            {
                Location loc_temp;
                //l의 기본값 넣는 부분 추가할 것
                loc_temp.root = split_result[1];
                while (1)
                {
                    getline(config_fd, line);
                    split_result = util::ft_split(line, "\t ");
                    if (split_result[0] == "}"){
                        _server_list.back().loc.push_back(loc_temp);
                        break ;
                    }
                    else if (split_result[0] == "return")
                    {//배열 크기가 3이 아닌 2일때 예외처리 할 것
                        loc_temp.redirection = split_result[1];
                        loc_temp.index = split_result[2];
                    }
                    else if (split_result[0] == "autoindex")
                    {
                        util::remove_last_semicolon(split_result[1]);
                        loc_temp.autoindex = (split_result[1] == "on");
                    }
                }
            }
            else if (split_result[0] == "}")
                continue;
            else
            {
                // error
            }

		//내용물 채우고 마지막 } 만나면 나오기
		//함수호출 끝
    	}
        config_fd.close();
        return (true);
    }

    //서버포트들을 개방하는 메소드.
    void open_ports()
    {
        for (int i(0);i < this->get_server_list().size();i++)
        {
            this->get_server_list()[i].open_port();
        }
    }

    //서버들을 감지목록에 추가하는 메소드.
    void regist_servers_to_kq()
    {
        for (int i(0);i < this->get_server_list().size();i++)
        {
            EV_SET(g_detects, this->get_server_list()[i], EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
        }
    }

    //서버를 실행하는 메소드.
    void start()
    {
        int kq_fd; //커널큐 fd.
        int detected_count; //감지된 이벤트 갯수.
        std::vector<struct kevent> detecteds; //감지 된 이벤트벡터.
        detecteds.reserve(DETECT_SIZE); //오버헤드 방지.

        // struct kevent k_set;
        /**
        EV_SET(이벤트구조체 &k_set,
            감지할fd,
            감지되면 설정될 플래그 EVFILT_READ 또는 EVFILT_WRITE,
            "이벤트추가" 및 "이벤트반환"매크로 EV_ADD | EV_ENABLE,
            필터플레그값 0,
            필터 데이터값 0,
            사용자 정의 데이터 NULL);
        **/

        this->open_ports(); //서버포트 열기
        this->regist_servers_to_kq(); //감지목록에 서버들 추가.
        kq_fd = kqueue();
        while ("soo-je-webserv")
        {
            try
            {
                detected_count = kevent(kq_fd, g_detects, g_detects.size(), detecteds, DETECT_SIZE, NULL);
                // g_detects.clear();
                for (int i(0); i < detected_count; i++)
                {
                    if (detecteds[i].flags & EVFILT_READ) //감지된 이벤트가 "읽기가능"일 때.
                    {
                        //감지된 fd가 정규파일인지 서버인지 클라이언트꺼인지 검사한다.
                        bool used = false; //찾았는지 여부.
                        
                        for (int j(0); j < get_server_list().size(); j++)
                        {   //감지된 fd가 서버쪽 일 때.
                            // if (g_io_infos[detecteds[i].ident].getType() == "server")
                            if (detecteds[i].ident == get_server_list()[j]->fd)
                            {
                                Client new_client = Client();
                                new_client.setSocket_fd(get_server_list()[j].accept_client()); //브라우저의 연결을 수락.
                                // g_io_infos[new_client] = IO_manager(new_client, "client", 0);                        
                                //감지목록에 등록. 
                                EV_SET(g_detects, new_client, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
                                /**EV_SET(이벤트구조체 &k_set,
                                    감지할fd,
                                    감지되면 설정될 플래그 EVFILT_READ 또는 EVFILT_WRITE,
                                    "이벤트추가" 및 "이벤트반환"매크로 EV_ADD | EV_ENABLE,
                                    필터플레그값 0,
                                    필터 데이터값 0,
                                    사용자 정의 데이터 NULL);
                                **/
                                this->set_client_list(new_client);
                                used = true;
                                break;
                            }
                        }
                        if (used == true)
                            continue;
                        for (std::vector<Client>::iterator it(get_client_list().begin()); it != get_client_list().end(); it++)
                        {   //감지된 fd가 클라쪽 일 때.
                            if (detecteds[i].ident == (*it).getSocket_fd())
                            {
                                int result = (*it).recv_data();
                                if (result == FAIL)
                                {
                                    //감지목록에서도 지운다. -> 처음부터 감지목록을 매회차 갱신으로 처리.
                                    std::vector<struct kevent>::iterator it2;
                                    for (it2 = g_detects.rbegin();it2 != g_detects.rend(); it2++)
                                    {
                                        if ((*it2).ident == (*it).getSocket_fd())
                                            g_detects.erase(it2);
                                    }
                                    //kq에서 읽기가능이라고 했는데도 데이터를 읽을 수 없다면 삭제한다.
                                    this->get_client_list().erase(it);
                                    
                                }
                                else if (result == RECV_ALL) //모두수신받았을 때.
                                {
                                    //수신받은 request데이터 파싱.
                                    (*it).parse_request();
                                    //파싱된 데이터에 cgi요청이없거나 잘못된형식일때.
                                    if ((*it).check_need_cgi() == false)
                                    {
                                        (*it).init_response(); //클라이언트는 파싱한 데이터로 응답클래스를 초기화한다.
                                        //감지목록에 클라이언트 소켓을 "쓰기가능감지"로 등록. (추후 브라우저에 데이터 보낼예정)
                                        EV_SET(g_detects, (*it).getSocket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
                                    }
                                    else //cgi요청이 있을 때. (POST)
                                    {
                                        //파싱된 데이터에 cgi요청이 있으면 fork로 보내고 파생된result파일을 읽도록 kq에 등록.
                                        //(추후 fd가 파일인지 검사하는 구간에서 파생해준client를 file_fd로 찾은후 response제작).
                                    }
                                }
                                used = true
                                break;
                            }
                        }
                        //감지된 fd가 파일쪽 일 때.
                        if (used == false)
                        {
                            for (std::vector<Client>::iterator it(get_client_list().begin());it != get_client_list().end();it++)
                            {   //먼저 어떤 클라이언트의 파일인지 찾는다.
                                if (detecteds[i].ident == (*it).getFile_fd())
                                {
                                    //클라이언트객체는 파일을 읽는다
                                    int result = (*it).read_file();
                                    if (result == FAIL) //파일 읽기 오류났을 때.
                                    {
                                        //감지목록에서도 지운다.
                                        std::vector<struct kevent>::iterator it2;
                                        for (it2 = g_detects.rbegin();it2 != g_detects.rend(); it2++)
                                        {
                                            if ((*it2).ident == (*it).getFile_fd())
                                                g_detects.erase(it2);
                                        }
                                        //**클라이언트의 파일 다운로드 상황을 완료로 처리해야할지도? 한다면 변수사용으로 처리 필요.
                                        //****여기에 응답 에러상태코드를 설정하는 부분을 넣어야 할 수도 있다.
                                        (*it).init_response(); //클라이언트는 파싱한 데이터로 응답클래스를 초기화한다.
                                    }
                                    else if (result == RECV_ALL) //모두수신받았을 때.
                                    {
                                        (*it).init_response(); //클라이언트는 파싱한 데이터로 응답클래스를 초기화한다.
                                    }
                                    ////////////////
                                    break;
                                }
                            }
                        }
                    }
                    else if (detecteds[i].flags & EVFILT_WRITE) //감지된 이벤트가 "쓰기가능"일 때.
                    {
                        bool used = false; //찾았는지 여부.
                        for (std::vector<Client>::iterator it(get_client_list().begin()); it != get_client_list().end(); it++)
                        {   //감지된 fd가 클라쪽 일 때.
                            if (detecteds[i].ident == (*it).getSocket_fd())
                            {
                                //클라이언트 객체가 완성된 response데이터를 전송.
                                (*it).send_data();
                                //**버퍼를 사용해서 BLOCK되지 않도록 한다. (나눠보내기)
                                used = true
                                break;
                            }
                        }
                        //감지된 fd가 파일쪽 일 때. (POST)
                        if (used == false)
                        {
                            //파일 구조체에 담긴 정보로 파일을 작성한다.
                            //쓰기가 완료되면 주인 클라이언트객체에게 알린다.
                        }
                    }
                    else
                    {
                        //error
                    }
                }
                // this->regist_servers_to_kq();
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
        }
    }
};

//**서버의 포트가 열리면 EV_SET해줘야 함.
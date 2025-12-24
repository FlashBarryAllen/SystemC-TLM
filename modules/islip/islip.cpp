#include "islip.h"

islip::islip(int input_num, int output_num)
{
    m_num_port = std::max(input_num, output_num);
    m_gi.resize(m_num_port);
    m_ai.resize(m_num_port);
    m_ql.resize(m_num_port * m_num_port);
    m_accept.resize(m_num_port * m_num_port);
    m_request.resize(m_num_port * m_num_port);
    m_grant.resize(m_num_port * m_num_port);

    init_priority_ptr();
}

islip::~islip()
{

}

void islip::init_priority_ptr()
{
    int i;    
    for (i = 0; i < m_num_port; i++)
    {
        m_ai.at(i) = i;
        m_gi.at(i) = i;
    }
}

void islip::set_ql(int i, int j)
{
    m_ql.at(i * m_num_port + j) = true;
}

void islip::islip_sch()
{
    send_request();
    do_grant();
    do_accept();
    update_priority_ptr();
}

void islip::init()
{
    int i,j;
    for (i = 0; i < m_num_port; i++)
    {
        for (j = 0; j < m_num_port; j++)
        {                //set these variables 1 when
            m_accept.at(i * m_num_port + j)  = false;    //input port i accepts the grant from output port j
            m_request.at(i * m_num_port + j) = false;    //input port i requests for output port j
            m_grant.at(i * m_num_port + j)   = false;     //input port i granted by output port j
            m_ql.at(i * m_num_port + j) = false;
        }
    }

    sch_result.clear();
}

void islip::send_request()
{
    int i,j;
    for (i = 0; i < m_num_port; i++)
    {
        for (j = 0; j < m_num_port; j++)
        {
            if (m_ql.at(i * m_num_port + j)) 
            //input i send s a request to output j
            //if it has a packet to send
            {
                m_request.at(i * m_num_port + j) = true;
            }
        }
    }    
}

void islip::do_grant()
{
    int i,j;
    for (j = 0; j < m_num_port; j++)
    {
        i = m_gi.at(j);
        do 
        {
            if (m_request.at(i * m_num_port + j))
            {
                m_grant.at(i * m_num_port + j) = 1;
                break;
            }
            i = (i + 1) % m_num_port;
        }
        while ( i != m_gi.at(j) ); 
    }
}

void islip::do_accept()
{
    int i,j;
    int tmp_ai;
    for (i = 0; i < m_num_port; i++)
    {
        j = m_ai.at(i);
        tmp_ai = m_ai.at(i);
        do 
        {
            if (m_grant.at(i * m_num_port + j))
            {
                m_accept.at(i * m_num_port + j) = 1;
                m_ai.at(i) = (j + 1) % m_num_port;
                break;
            }
            j = (j + 1) % m_num_port;
        }
        while ( j != tmp_ai );
    }
}

void islip::update_priority_ptr()
{
    int i,j;
    for (j = 0 ; j < m_num_port; j ++)
    {
        for (i = 0; i < m_num_port; i++ )
        {
            if ( m_accept.at(i * m_num_port + j)) //Update gi only if its grant is accepted
            {
                m_gi.at(j) = (i + 1) % m_num_port;
                sch_result.emplace_back(std::make_pair(i, j));
            }
        }
    }
}
#include "booksim_islip.h"

islip_booksim::islip_booksim(int input_num, int output_num, int iterations)
{
    m_num_port = std::max(input_num, output_num);
    m_gi.resize(m_num_port);
    m_ai.resize(m_num_port);
    m_ql.resize(m_num_port * m_num_port);
    m_accept.resize(m_num_port * m_num_port);
    m_request.resize(m_num_port * m_num_port);
    m_grant.resize(m_num_port * m_num_port);
    m_iterations = iterations;
    m_iter_cnt = 0;

    m_input_occupied.resize(m_num_port, false);
    m_output_occupied.resize(m_num_port, false);

    init_priority_ptr();
    init();
}

islip_booksim::~islip_booksim()
{

}

void islip_booksim::init_priority_ptr()
{
    int i;    
    for (i = 0; i < m_num_port; i++)
    {
        m_ai.at(i) = 0;
        m_gi.at(i) = 0;
    }
}

void islip_booksim::set_ql(int i, int j)
{
    m_ql.at(i * m_num_port + j) = true;
}

void islip_booksim::islip_sch()
{
    m_iter_cnt = 0;
    for (; m_iter_cnt < m_iterations; m_iter_cnt++)
    {
        send_request();
        do_grant();
        do_accept();
        update_priority_ptr();
    }
}

void islip_booksim::init()
{
    for (int input = 0; input < m_num_port; input++)
    {
        m_input_occupied.at(input) = false;
        for (int output = 0; output < m_num_port; output++)
        {
            m_accept.at(input * m_num_port + output)  = false;    //input port i accepts the grant from output port j
            m_request.at(input * m_num_port + output) = false;    //input port i requests for output port j
            m_grant.at(input * m_num_port + output)   = false;     //input port i granted by output port j
            m_ql.at(input * m_num_port + output) = false;
        }
    }

    for (int output = 0; output < m_num_port; output++)
    {
        m_output_occupied.at(output) = false;
    }

    sch_result.clear();
}

void islip_booksim::send_request()
{
    for (int input = 0; input < m_num_port; input++)
    {
        for (int output = 0; output < m_num_port; output++)
        {
            if (m_ql.at(input * m_num_port + output))
            {
                m_request.at(input * m_num_port + output) = true;
            }
        }
    }
}

void islip_booksim::do_grant()
{
    for (int output = 0; output < m_num_port; output++)
    {
        if (m_output_occupied.at(output))
        {
            continue;
        }

        int input = m_gi.at(output);
        do 
        {
            if (m_input_occupied.at(input))
            {
                input = (input + 1) % m_num_port;
                continue;
            }
            if (m_request.at(input * m_num_port + output))
            {
                m_grant.at(input * m_num_port + output) = 1;
                break;
            }
            input = (input + 1) % m_num_port;
        }
        while ( input != m_gi.at(output) ); 
    }
}

void islip_booksim::do_accept()
{
    for (int input = 0; input < m_num_port; input++)
    {
        if (m_input_occupied.at(input))
        {
            continue;
        }

        int output = m_ai.at(input);

        do 
        {
            if (m_output_occupied.at(output))
            {
                output = (output + 1) % m_num_port;
                continue;
            }
            if (m_grant.at(input * m_num_port + output))
            {
                m_accept.at(input * m_num_port + output) = 1;
                break;
            }
            output = (output + 1) % m_num_port;
        }
        while ( output !=  m_ai.at(input) );
    }
}

void islip_booksim::update_priority_ptr()
{
    for (int input = 0; input < m_num_port; input++)
    {
        if (m_input_occupied.at(input))
        {
            continue;
        }

        for (int output = 0; output < m_num_port; output++)
        {
            if (m_output_occupied.at(output))
            {
                continue;
            }

            if ( m_accept.at(input * m_num_port + output)) //Update gi only if its grant is accepted
            {
                if (m_iter_cnt == 0) //iSLIP logic: update gi only in the first iteration{
                {
                    m_gi.at(output) = (input + 1) % m_num_port;
                    m_ai.at(input) = (output + 1) % m_num_port;
                }

                m_input_occupied.at(input) = true;
                m_output_occupied.at(output) = true;
                
                sch_result.emplace_back(std::make_pair(input, output));
            }
        }
    }
}
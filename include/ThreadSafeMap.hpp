#ifndef THREADSAFEMAP_HPP
#define THREADSAFEMAP_HPP

template<typename _Key, typename _Tp,
    typename _Comp = std::less<_Key>,
    typename _Alloc = std::allocator<std::pair<const _Key, _Tp> > >
class ThreadSafeMap
{
public:
    typedef std::map<_Key, _Tp, _Comp, _Alloc> map_type;
    typedef typename map_type::key_type key_type;
    typedef typename map_type::mapped_type mapped_type;
    typedef typename map_type::value_type value_type;
    typedef typename map_type::key_compare key_compare;
    typedef typename map_type::allocator_type allocator_type;
    typedef typename map_type::reference reference;
    typedef typename map_type::const_reference const_reference;
    typedef typename map_type::pointer pointer;
    typedef typename map_type::const_pointer const_pointer;
    typedef typename map_type::iterator iterator;
    typedef typename map_type::const_iterator const_iterator;
    typedef typename map_type::size_type size_type;
    typedef typename map_type::difference_type difference_type;

    explicit ThreadSafeMap(const key_compare& __map = key_compare(),
        const allocator_type& __alloc = allocator_type()) :m_map(__map, __alloc) {}
    template<typename _InputIterator>
    ThreadSafeMap(_InputIterator __first, _InputIterator __last,
        const key_compare& __comp = key_compare(),
        const allocator_type& __alloc = allocator_type()) : m_map(__first, __last, __comp, __alloc) {}
    ThreadSafeMap(const map_type& v) : m_map(v) {}
    ThreadSafeMap(map_type&& rv) :m_map(std::move(rv)) {}
    virtual ~ThreadSafeMap()
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        m_map.clear();
    }

    iterator begin() noexcept
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.begin();
    }

    const_iterator begin() const noexcept
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.begin();
    }

    const_iterator cbegin() noexcept
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.cbegin();
    }

    iterator end() noexcept
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.end();
    }

    const_iterator end() const noexcept
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.end();
    }

    const_iterator cend() const noexcept
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.cend();
    }

    std::pair<iterator, bool> insert(const value_type& value)
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.insert(value);
    }

    iterator insert(const_iterator __hint, const value_type& __x)
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.insert(__hint, __x);
    }

    template<typename _InputIterator>
    void insert(_InputIterator __first, _InputIterator __last)
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        m_map.insert(__first, __last);
    }

    bool insert(const key_type& key, const value_type& value, bool cover = false)
    {
        std::lock_guard<std::mutex> locker(m_mutex);

        auto itor = m_map.find(key);
        if (itor != m_map.end() && cover)
        {
            m_map.erase(itor);
        }
        auto result = m_map.insert(std::pair<key_type, value_type>(key, value));
        return result.second;
    }

    iterator erase(const_iterator itor)
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.erase(itor);
    }

    iterator erase(iterator itor)
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.erase(itor);
    }

    size_type erase(const key_type& key)
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.erase(key);
    }

    iterator erase(const_iterator first, const_iterator last)
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.erase(first, last);
    }

    void clear() noexcept
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        m_map.clear();
    }

    void remove(const key_type& key)
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        auto find = m_map.find(key);
        if (find != m_map.end())
        {
            m_map.erase(find);
        }
    }

    iterator find(const key_type& key)
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.find(key);
    }

    const_iterator find(const key_type& key) const
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.find(key);
    }

    size_type count(const key_type& key) const
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.count(key);
    }

    mapped_type& operator[](const key_type& key)
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map[key];
    }

    mapped_type& operator[](key_type&& key)
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map[std::move(key)];
    }

    bool find(const key_type& key, value_type& value) const
    {
        std::lock_guard<std::mutex> locker(m_mutex);

        auto itor = m_map.find(key);
        auto found = itor != m_map.end();
        if (found)
        {
            value = itor->second;
        }
        return found;

    }

    int size()
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        return m_map.size();
    }


private:
    std::mutex m_mutex;
    std::map<_Key, _Tp, _Comp, _Alloc> m_map;

};
#endif
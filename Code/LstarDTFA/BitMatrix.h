#pragma once
#include <algorithm>
#include <stdexcept>
#include <vector>


namespace bit_matrix {

class BitMatrixColMajor
{
public:
    using block_t                     = std::uint64_t;
    static constexpr std::size_t BITS = 64;

    BitMatrixColMajor() = default;

    BitMatrixColMajor(std::size_t rows, std::size_t cols)
    {
        resize(rows, cols);
    }

    void resize(std::size_t new_rows, std::size_t new_cols)
    {
        const std::size_t new_bpc = blocks_per_col(new_rows);

        // Resize columns vector
        const std::size_t old_cols = m_cols;
        const std::size_t old_rows = m_rows;

        m_rows = new_rows;
        m_cols = new_cols;
        m_bpc  = new_bpc;

        if (m_cols < old_cols || m_rows < old_rows)
        {
            return;
        }

        const block_t fillv = block_t { 0 };

        // Existing columns: resize their block arrays
        for (std::size_t c = 0; c < old_cols; ++c)
        {
            m_data[c].resize(m_bpc, fillv);
        }

        // New columns: allocate and fill
        m_data.resize(m_cols);
        for (std::size_t c = old_cols; c < m_cols; ++c)
        {
            m_data[c].assign(m_bpc, fillv);
        }
    }

    [[nodiscard]] std::size_t rows() const noexcept
    {
        return m_rows;
    } // contexts

    [[nodiscard]] std::size_t cols() const noexcept
    {
        return m_cols;
    } // trees

    [[nodiscard]] std::size_t blocksPerColumn() const noexcept
    {
        return m_bpc;
    }

    void clear() noexcept
    {
        for (auto& col : m_data)
        {
            std::ranges::fill(col, block_t { 0 });
        }
    }

    [[nodiscard]] bool get(std::size_t r, std::size_t c) const
    {
        bounds_check(r, c);
        const auto [b, bit] = locate(r);
        return (m_data[c][b] >> bit) & block_t { 1 };
    }

    [[nodiscard]] std::vector<uint64_t> get_column(std::size_t c) const
    {
        bounds_check(0, c);
        return m_data[c];
        //const auto [b, bit] = locate(m_rows-1);

        //std::vector<bool> res;

        //for (size_t i = 0; i < b; i++)
        //{
        //    for (int j = 0; j < BITS;j++)
        //    {
        //        res.push_back((m_data[c][i] >> j) & block_t { 1 }); 
        //    }
        //}

        //for (int j = 0; j < m_rows%BITS; j++)
        //{
        //    res.push_back((m_data[c][b] >> j) & block_t { 1 });
        //}

        //return res;
    }

    void set(std::size_t r, std::size_t c, bool value)
    {
        bounds_check(r, c);
        const auto [b, bit] = locate(r);
        const block_t mask  = (block_t { 1 } << bit);
        if (value)
        {
            m_data[c][b] |= mask;
        }
        else
        {
            m_data[c][b] &= ~mask;
        }
    }

    void set_true(std::size_t r, std::size_t c)
    {
        bounds_check(r, c);
        const auto [b, bit]  = locate(r);
        m_data[c][b]        |= (block_t { 1 } << bit);
    }

    void set_false(std::size_t r, std::size_t c)
    {
        bounds_check(r, c);
        const auto [b, bit]  = locate(r);
        m_data[c][b]        &= ~(block_t { 1 } << bit);
    }

    // Fast access to an entire column (tree): blocks over all contexts
    [[nodiscard]] const std::vector<block_t>& column(std::size_t c) const
    {
        if (c >= m_cols)
        {
            throw std::out_of_range("BitMatrixColMajor::column out of range");
        }
        return m_data[c];
    }

    [[nodiscard]] std::vector<block_t>& column(std::size_t c)
    {
        if (c >= m_cols)
        {
            throw std::out_of_range("BitMatrixColMajor::column out of range");
        }
        return m_data[c];
    }

    [[nodiscard]] bool column_equal(std::size_t c1, std::size_t c2) const
    {
        if (c1 >= m_cols || c2 >= m_cols)
        {
            throw std::out_of_range("BitMatrixColMajor::column out of range");
        }

        const auto [b, bit] = locate(m_rows - 1);

        for (size_t i = 0; i < b; i++)
        {
            if (m_data[c1][i] != m_data[c2][i])
            {
                return false;
            }
        }

        for (size_t i = 0; i <= bit; i++)
        {
            if (((m_data[c1][b] ^ m_data[c2][b]) >> bit) & block_t { 1 })
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] int column_first_differ_index(std::size_t c1, std::size_t c2) const
    {
        if (c1 >= m_cols || c2 >= m_cols)
        {
            throw std::out_of_range("BitMatrixColMajor::column out of range");
        }

        const auto [b, bit] = locate(m_rows - 1);

        for (size_t i = 0; i < b; i++)
        {
            if (m_data[c1][i] != m_data[c2][i])
            {
                const auto xor_res = (m_data[c1][i] ^ m_data[c2][i]);
                for (size_t j = 0; j < 64; j++)
                {
                    if ((xor_res >> j) & block_t { 1 })
                    {
                        return i*64+j;
                    }
                }
            }
        }

        for (size_t i = 0; i <= bit; i++)
        {
            if (((m_data[c1][b] ^ m_data[c2][b]) >> i) & block_t { 1 })
            {
                return b*64+i;
            }
        }

        return -1;
    }


private:
    std::size_t m_rows = 0;
    std::size_t m_cols = 0;
    std::size_t m_bpc  = 0; // blocks per column

    // m_data[col][block]
    std::vector<std::vector<block_t>> m_data;

    static std::size_t blocks_per_col(std::size_t rows) noexcept
    {
        return (rows + BITS - 1) / BITS;
    }

    static std::pair<std::size_t, std::size_t> locate(std::size_t r) noexcept
    {
        return { r / BITS, r % BITS };
    }

    void bounds_check(std::size_t r, std::size_t c) const
    {
        if (r >= m_rows || c >= m_cols)
        {
            throw std::out_of_range("BitMatrixColMajor index out of range");
        }
    }
};

} // namespace bit_matrix
